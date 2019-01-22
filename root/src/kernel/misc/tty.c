#include <stdint.h>
#include <stddef.h>
#include <misc/tty.h>
#include <lib/klib.h>
#include <misc/vbe.h>
#include <misc/vga_textmode.h>
#include <misc/vbe_tty.h>
#include <lib/cmdline.h>
#include <misc/kbd.h>
#include <devices/dev.h>
#include <lib/lock.h>

static int use_vbe = 0;

lock_t termios_lock = 1;
struct termios_t termios = {0};

int tty_tcsetattr(int optional_actions, struct termios_t *new_termios) {
    spinlock_acquire(&termios_lock);
    kmemcpy(&termios, new_termios, sizeof(struct termios_t));
    spinlock_release(&termios_lock);
    return 0;
}

void tty_putchar(char c) {
    tty_write(0, &c, 0, 1);
}

int tty_write(int magic, const void *data, uint64_t loc, size_t count) {
    char *buf = (char *)data;

    if (use_vbe) {
        if (!vbe_available || !vbe_tty_available) {
            text_write(buf, count);
        } else {
            vbe_tty_write(buf, count);
        }
    } else {
        text_write(buf, count);
    }

    return (int)count;
}

int tty_read(int magic, void *data, uint64_t loc, size_t count) {
    int res = (int)kbd_read(data, count);

    return res;
}

/* Stub for now */
static int tty_flush(int dev) {
    return 1;
}

void init_tty(void) {
    char *tty_cmdline;

    spinlock_acquire(&termios_lock);
    termios.c_lflag = (ICANON | ECHO);
    spinlock_release(&termios_lock);

    if ((tty_cmdline = cmdline_get_value("display"))) {
        if (!kstrcmp(tty_cmdline, "vga")) {
            use_vbe = 0;
        } else if (!kstrcmp(tty_cmdline, "vbe")) {
            use_vbe = 1;
        } else {
            for (;;);
        }
    } else {
        use_vbe = 1;
    }

    dev_t dev = device_add("tty", 0xdead, 0, &tty_read, &tty_write, &tty_flush);
    if (dev == -1) {
        return;
    }

    return;
}
