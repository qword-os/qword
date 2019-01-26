#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <sys/apic.h>
#include <misc/tty.h>
#include <lib/lock.h>

#define MAX_CODE 0x57
#define CAPSLOCK 0x3a
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT_REL 0xb6
#define LEFT_SHIFT_REL 0xaa
#define CTRL 0x1d
#define CTRL_REL 0x9d

#define KBD_BUF_SIZE 2048
#define BIG_BUF_SIZE 65536

static size_t kbd_buf_i = 0;
static char kbd_buf[KBD_BUF_SIZE];

static size_t big_buf_i = 0;
static char big_buf[BIG_BUF_SIZE];

static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;
static int extra_scancodes = 0;

static const uint8_t ascii_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

void init_kbd(void) {
    io_apic_set_mask(0, 1, 1);
    return;
}

static lock_t kbd_read_lock = 1;

int kbd_read(char *buf, size_t count) {
    int wait = 1;

    while (!spinlock_test_and_acquire(&kbd_read_lock)) {
        yield(10);
    }

    for (size_t i = 0; i < count; ) {
        if (big_buf_i) {
            buf[i++] = big_buf[0];
            big_buf_i--;
            for (size_t j = 0; j < big_buf_i; j++) {
                big_buf[j] = big_buf[j+1];
            }
            wait = 0;
        } else {
            if (wait) {
                spinlock_release(&kbd_read_lock);
                yield(10);
                while (!spinlock_test_and_acquire(&kbd_read_lock)) {
                    yield(10);
                }
            } else {
                spinlock_release(&kbd_read_lock);
                return (int)i;
            }
        }
    }

    spinlock_release(&kbd_read_lock);
    return (int)count;
}

static void add_to_buf(uint8_t c) {
    spinlock_acquire(&termios_lock);

    if (termios.c_lflag & ICANON) {
        switch (c) {
            case '\n':
                if (kbd_buf_i == KBD_BUF_SIZE)
                    break;
                kbd_buf[kbd_buf_i++] = c;
                if (termios.c_lflag & ECHO)
                    tty_putchar(c);
                for (size_t i = 0; i < kbd_buf_i; i++) {
                    if (big_buf_i == BIG_BUF_SIZE)
                        break;
                    big_buf[big_buf_i++] = kbd_buf[i];
                }
                kbd_buf_i = 0;
                goto out;
            case '\b':
                if (!kbd_buf_i)
                    break;
                kbd_buf[--kbd_buf_i] = 0;
                if (termios.c_lflag & ECHO) {
                    tty_putchar('\b');
                    tty_putchar(' ');
                    tty_putchar('\b');
                }
                goto out;
        }
    }

    if (termios.c_lflag & ICANON) {
        if (kbd_buf_i == KBD_BUF_SIZE)
            goto out;
        kbd_buf[kbd_buf_i++] = c;
    } else {
        if (big_buf_i == BIG_BUF_SIZE)
            goto out;
        big_buf[big_buf_i++] = c;
    }

    if (is_printable(c) && (termios.c_lflag & ECHO))
        tty_putchar(c);

out:
    spinlock_release(&termios_lock);
    return;
}

void kbd_handler(uint8_t input_byte) {
    char c = '\0';

    spinlock_acquire(&kbd_read_lock);

    if (input_byte == 0xe0) {
        extra_scancodes = 1;
        goto out;
    }

    if (extra_scancodes) {
        // extra scancodes
        extra_scancodes = 0;
        switch (input_byte) {
            case 0x48:
                // cursor up
                add_to_buf('\e');
                add_to_buf('[');
                add_to_buf('A');
                goto out;
            case 0x4B:
                // cursor left
                add_to_buf('\e');
                add_to_buf('[');
                add_to_buf('D');
                goto out;
            case 0x50:
                // cursor down
                add_to_buf('\e');
                add_to_buf('[');
                add_to_buf('B');
                goto out;
            case 0x4D:
                // cursor right
                add_to_buf('\e');
                add_to_buf('[');
                add_to_buf('C');
                goto out;
            case 0x49:
                // pgup
                goto out;
            case 0x51:
                // pgdown
                goto out;
            case 0x53:
                // delete
                goto out;
            case CTRL:
                ctrl_active = 1;
                goto out;
            case CTRL_REL:
                ctrl_active = 0;
                goto out;
            default:
                break;
        }
    }

    if (ctrl_active) {
        switch (input_byte) {
            case 0x2e:
                goto out;
            default:
                break;
        }
    }

    switch (input_byte) {
        case LEFT_SHIFT:
        case RIGHT_SHIFT:
            shift_active = 1;
            goto out;
        case LEFT_SHIFT_REL:
        case RIGHT_SHIFT_REL:
            shift_active = 0;
            goto out;
        case CTRL:
            ctrl_active = 1;
            goto out;
        case CTRL_REL:
            ctrl_active = 0;
            goto out;
        case CAPSLOCK:
            capslock_active = !capslock_active;
            goto out;
        default:
            break;
    }

    /* Assign the correct character for this scancode based on modifiers */
    if (input_byte < MAX_CODE) {
        if (!capslock_active && !shift_active)
            c = ascii_nomod[input_byte];
        else if (!capslock_active && shift_active)
            c = ascii_shift[input_byte];
        else if (capslock_active && shift_active)
            c = ascii_shift_capslock[input_byte];
        else
            c = ascii_capslock[input_byte];
    } else {
        goto out;
    }

    add_to_buf(c);

out:
    spinlock_release(&kbd_read_lock);
    return;
}
