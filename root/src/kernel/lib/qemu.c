#include <stdint.h>
#include <stddef.h>
#include <lib/qemu.h>
#include <lib/cio.h>
#include <lib/lock.h>

static lock_t qemu_debug_lock = 1;

void qemu_debug_puts(const char *str) {
    spinlock_acquire(&qemu_debug_lock);
    for (size_t i = 0; str[i]; i++)
        port_out_b(0xe9, str[i]);
    spinlock_release(&qemu_debug_lock);
}

void qemu_debug_putc(char c) {
    spinlock_acquire(&qemu_debug_lock);
    port_out_b(0xe9, c);
    spinlock_release(&qemu_debug_lock);
}
