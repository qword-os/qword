#include <stdint.h>
#include <stddef.h>
#include <lib/qemu.h>
#include <lib/cio.h>
#include <lib/lock.h>

static lock_t qemu_debug_lock = new_lock;
static int block = 0;

void qemu_debug_puts_urgent(const char *str) {
    locked_inc(&block);
    for (size_t i = 0; str[i]; i++)
        port_out_b(0xe9, str[i]);
    locked_dec(&block);
}

void qemu_debug_puts(const char *str) {
    spinlock_acquire(&qemu_debug_lock);
    for (size_t i = 0; str[i]; i++) {
        while (locked_read(int, &block));
        port_out_b(0xe9, str[i]);
    }
    spinlock_release(&qemu_debug_lock);
}

void qemu_debug_putc(char c) {
    spinlock_acquire(&qemu_debug_lock);
    while (locked_read(int, &block));
    port_out_b(0xe9, c);
    spinlock_release(&qemu_debug_lock);
}
