#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <sys/apic.h>
#include <devices/term/tty/tty.h>
#include <lib/lock.h>
#include <lib/event.h>
#include <sys/irq.h>
#include <lib/cio.h>
#include <proc/task.h>
#include <lib/signal.h>

#define MAX_CODE 0x57
#define CAPSLOCK 0x3a
#define LEFT_ALT 0x38
#define LEFT_ALT_REL 0xb8
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT_REL 0xb6
#define LEFT_SHIFT_REL 0xaa
#define CTRL 0x1d
#define CTRL_REL 0x9d

static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;
static int alt_active = 0;
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

int tty_read(int tty, void *void_buf, uint64_t unused, size_t count) {
    (void)unused;

    if (!tty_ready)
        return 0;
    if (ttys[tty].tcioff) {
        errno = EINVAL;
        return -1;
    }

    char *buf = void_buf;
    int wait = 1;

    while (!spinlock_test_and_acquire(&ttys[tty].read_lock)) {
        if (event_await(&ttys[tty].kbd_event)) {
            // signal is aborting us, bail
            errno = EINTR;
            return -1;
        }
    }

    for (size_t i = 0; i < count; ) {
        if (ttys[tty].big_buf_i) {
            buf[i++] = ttys[tty].big_buf[0];
            ttys[tty].big_buf_i--;
            for (size_t j = 0; j < ttys[tty].big_buf_i; j++) {
                ttys[tty].big_buf[j] = ttys[tty].big_buf[j+1];
            }
            wait = 0;
        } else {
            if (wait) {
                spinlock_release(&ttys[tty].read_lock);
                do {
                    if (event_await(&ttys[tty].kbd_event)) {
                        // signal is aborting us, bail
                        errno = EINTR;
                        return -1;
                    }
                } while (!spinlock_test_and_acquire(&ttys[tty].read_lock));
            } else {
                spinlock_release(&ttys[tty].read_lock);
                return (int)i;
            }
        }
    }

    spinlock_release(&ttys[tty].read_lock);
    return (int)count;
}

static void add_to_buf_char(int tty, char c) {
    spinlock_acquire(&ttys[tty].read_lock);

    if (ttys[tty].termios.c_lflag & ICANON) {
        switch (c) {
            case '\n':
                if (ttys[tty].kbd_buf_i == KBD_BUF_SIZE)
                    goto out;
                ttys[tty].kbd_buf[ttys[tty].kbd_buf_i++] = c;
                if (ttys[tty].termios.c_lflag & ECHO)
                    put_char(tty, c);
                for (size_t i = 0; i < ttys[tty].kbd_buf_i; i++) {
                    if (ttys[tty].big_buf_i == BIG_BUF_SIZE)
                        goto out;
                    ttys[tty].big_buf[ttys[tty].big_buf_i++] = ttys[tty].kbd_buf[i];
                }
                ttys[tty].kbd_buf_i = 0;
                goto out;
            case '\b':
                if (!ttys[tty].kbd_buf_i)
                    goto out;
                ttys[tty].kbd_buf[--ttys[tty].kbd_buf_i] = 0;
                if (ttys[tty].termios.c_lflag & ECHO) {
                    put_char(tty, '\b');
                    put_char(tty, ' ');
                    put_char(tty, '\b');
                }
                goto out;
        }
    }

    if (ttys[tty].termios.c_lflag & ICANON) {
        if (ttys[tty].kbd_buf_i == KBD_BUF_SIZE)
            goto out;
        ttys[tty].kbd_buf[ttys[tty].kbd_buf_i++] = c;
    } else {
        if (ttys[tty].big_buf_i == BIG_BUF_SIZE)
            goto out;
        ttys[tty].big_buf[ttys[tty].big_buf_i++] = c;
    }

    if (is_printable(c) && ttys[tty].termios.c_lflag & ECHO)
        put_char(tty, c);

out:
    spinlock_release(&ttys[tty].read_lock);
    return;
}

static void add_to_buf(int tty, const char *s, size_t cnt) {
    for (size_t i = 0; i < cnt; i++) {
        if (ttys[tty].termios.c_lflag & ISIG) {
            // accept signal characters
            if (s[i] == ttys[tty].termios.c_cc[VINTR]) {
                // sigint
                // XXX fix hardcoded PID number
                kill(7, SIGINT);
                continue;
            }
        }
        add_to_buf_char(tty, s[i]);
    }
    event_trigger(&ttys[tty].kbd_event);
}

// keyboard handler worker
static void kbd_handler(void *unused) {
    (void)unused;

await:
    event_await(&irq[1]);
    uint8_t input_byte = port_in_b(0x60);

    char c = '\0';

    if (input_byte == 0xe0) {
        extra_scancodes = 1;
        goto out;
    }

    if (extra_scancodes) {
        extra_scancodes = 0;

        // extra scancodes
        switch (input_byte) {
            case CTRL:
                ctrl_active = 1;
                goto out;
            case CTRL_REL:
                ctrl_active = 0;
                goto out;
            default:
                break;
        }

        // figure out correct escape sequence
        int decckm = ttys[current_tty].decckm;

        // extra scancodes
        switch (input_byte) {
            case 0x47:
                // home
                add_to_buf(current_tty, decckm ? "\e[H" : "\eOH", 3);
                goto out;
            case 0x4f:
                // end
                add_to_buf(current_tty, decckm ? "\e[F" : "\eOF", 3);
                goto out;
            case 0x48:
                // cursor up
                add_to_buf(current_tty, decckm ? "\e[A" : "\eOA", 3);
                goto out;
            case 0x4B:
                // cursor left
                add_to_buf(current_tty, decckm ? "\e[D" : "\eOD", 3);
                goto out;
            case 0x50:
                // cursor down
                add_to_buf(current_tty, decckm ? "\e[B" : "\eOB", 3);
                goto out;
            case 0x4D:
                // cursor right
                add_to_buf(current_tty, decckm ? "\e[C" : "\eOC", 3);
                goto out;
            case 0x49:
                // pgup
                add_to_buf(current_tty, decckm ? "\e[5~" : "\eO5~", 4);
                goto out;
            case 0x51:
                // pgdown
                add_to_buf(current_tty, decckm ? "\e[6~" : "\eO6~", 4);
                goto out;
            case 0x53:
                // delete
                add_to_buf(current_tty, decckm ? "\e[3~" : "\eO3~", 4);
                goto out;
            default:
                break;
        }
    }

    switch (input_byte) {
        case LEFT_ALT:
            alt_active = 1;
            goto out;
        case LEFT_ALT_REL:
            alt_active = 0;
            goto out;
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

    if (ctrl_active && alt_active) {
        // ctrl-alt combos
        if (input_byte >= 0x3b && input_byte <= 0x40) {
            // ctrl-alt [f1-f6]
            current_tty = input_byte - 0x3b;
            refresh(current_tty);
            goto out;
        }
    }

    if (ctrl_active) {
        switch (input_byte) {
            default:
                break;
        }
    }

    /* Assign the correct character for this scancode based on modifiers */
    if (input_byte < MAX_CODE) {
        if (ctrl_active)
            // TODO: Proper caret notation would be nice
            c = ascii_capslock[input_byte] - ('?' + 1);
        else if (!capslock_active && !shift_active)
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

    add_to_buf(current_tty, &c, 1);

out:
    goto await;
}
