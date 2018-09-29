#include <stdint.h>
#include <stddef.h>
#include <tty.h>
#include <klib.h>
#include <vbe.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <cmdline.h>
#include <kbd.h>
#include <dev.h>
#include <lock.h>

/* TODO: handle multiple ttys */
static int use_vbe = 0;

static lock_t tty_io_lock = 1;

static int tty_write(int magic, const void *data, uint64_t loc, size_t count) {
    spinlock_acquire(&tty_io_lock);

    char *buf = (char *)data;

    for (size_t i = 0; i < count; i++) {
        tty_putchar(buf[i]);
    }

    spinlock_release(&tty_io_lock);
    return (int)count;
}

static int tty_read(int magic, void *data, uint64_t loc, size_t count) {
    spinlock_acquire(&tty_io_lock);

    kbd_read(data, count);

    spinlock_release(&tty_io_lock);
    return (int)count;
}

/* Stub for now */
static int tty_flush(int dev) {
    return 0;
}

void init_tty(void) {
    char *tty_cmdline;

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

void tty_putchar(char c) {
    if (use_vbe) {
        if (!vbe_available || !vbe_tty_available) {
            text_putchar(c);
        } else {
            vbe_tty_putchar(c);
        }
    } else {
        text_putchar(c);
    }

    return;
}
