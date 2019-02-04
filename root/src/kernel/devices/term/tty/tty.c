#include <stdint.h>
#include <stddef.h>
#include <devices/term/tty/tty.h>
#include <lib/klib.h>
#include <misc/vbe.h>
#include <devices/term/tty/vbe_tty.h>
#include <lib/cmdline.h>
#include <devices/term/tty/kbd.h>
#include <devices/dev.h>
#include <lib/lock.h>

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
    (void)magic;
    char *buf = (char *)data;

    if (vbe_tty_available)
        vbe_tty_write(buf, count);

    return (int)count;
}

int tty_read(int magic, void *data, uint64_t loc, size_t count) {
    (void)magic;
    int res = (int)kbd_read(data, count);

    return res;
}

static int tty_flush(int dev) {
    return 1;
}

void init_tty(void) {
    char *tty_cmdline;

    spinlock_acquire(&termios_lock);
    termios.c_lflag = (ICANON | ECHO);
    spinlock_release(&termios_lock);

    struct device_t device = {0};
    kstrcpy(device.name, "tty");
    device.intern_fd = 0;
    device.size = 0;
    device.calls.read = tty_read;
    device.calls.write = tty_write;
    device.calls.flush = tty_flush;
    dev_t dev = device_add(&device);

    if (dev == -1) {
        return;
    }

    return;
}
