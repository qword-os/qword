#include <stdint.h>
#include <lib/lock.h>
#include <lib/cio.h>
#include <misc/serial.h>

static uint16_t serial_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };

void init_com1(void) {
    port_out_b(serial_ports[0] + 1, 0x00);
    port_out_b(serial_ports[0] + 3, 0x80);
    port_out_b(serial_ports[0] + 0, 0x03);
    port_out_b(serial_ports[0] + 1, 0x00);
    port_out_b(serial_ports[0] + 3, 0x03);
    port_out_b(serial_ports[0] + 2, 0xc7);
    port_out_b(serial_ports[0] + 4, 0x0b);
    return;
}

static lock_t com1_lock = new_lock;

uint8_t com1_read(void) {
    spinlock_acquire(&com1_lock);
    while (!(port_in_b(serial_ports[0] + 5) & 0x01));
    volatile uint8_t ret = port_in_b(serial_ports[0]);
    spinlock_release(&com1_lock);
    return ret;
}

void com1_write(uint8_t data) {
    spinlock_acquire(&com1_lock);
    while (!(port_in_b(serial_ports[0] + 5) & 0x20));
    port_out_b(serial_ports[0], data);
    spinlock_release(&com1_lock);
    return;
}
