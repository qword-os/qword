#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <pic.h>

#define MAX_CODE 0x57
#define CAPSLOCK 0x3a
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT_REL 0xb6
#define LEFT_SHIFT_REL 0xaa
#define LEFT_CTRL 0x1d
#define LEFT_CTRL_REL 0x9d
#define KB_BUF_SIZE 256

size_t buf_index = 0;
static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;
char tty_bufs[8][256];

static const char ascii_capslock[] = {
    '\0', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const char ascii_shift[] = {
    '\0', '?', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_shift_capslock[] = {
    '\0', '?', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_nomod[] = {
    '\0', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

void init_kbd(void) {
    pic_set_mask(1, 1); 
    return;
}

void kbd_handler(uint8_t input_byte) {   
    char c = '\0';

    if (ctrl_active) {
        switch (input_byte) {
            case 0x2e:
                return;
            default:
                break;
        }
    }

    /* Update modifiers */
    if (input_byte == CAPSLOCK) {
        /* TODO LED stuff */
        return;
    } else if (input_byte == LEFT_SHIFT || input_byte == RIGHT_SHIFT || input_byte == LEFT_SHIFT_REL || input_byte == RIGHT_SHIFT_REL)
        shift_active = !shift_active;
    else if (input_byte == LEFT_CTRL || input_byte == LEFT_CTRL_REL)
        ctrl_active = !ctrl_active;
    else if (buf_index < KB_BUF_SIZE) {
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

            tty_bufs[0][buf_index++] = c;
        }
    }

    return;
}
