#include <stdint.h>
#include <stddef.h>
#include <qemu.h>
#include <cio.h>

void qemu_debug_puts(const char *str) {
    for (size_t i = 0; str[i]; i++)
        port_out_b(0xe9, str[i]);
}

void qemu_debug_putc(char c) {
    port_out_b(0xe9, c);
}
