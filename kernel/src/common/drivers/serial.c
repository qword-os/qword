#include <cio.h>
#include <serial.h>

#define PORT 0x3F8

void serial_init(void) {
   port_out_b(PORT + 1, 0x00);
   port_out_b(PORT + 3, 0x80);
   port_out_b(PORT + 0, 0x03);
   port_out_b(PORT + 1, 0x00);
   port_out_b(PORT + 3, 0x03);
   port_out_b(PORT + 2, 0xC7);
   port_out_b(PORT + 4, 0x0B);
}

int can_read() {
    return port_in_b(PORT + 5) & 1;
}

char serial_read() {
    while (can_read() == 0) {}

    port_in_b(PORT);
}

int can_transmit_empty() {
    return port_in_b(PORT + 5) & 0x20;
}

void serial_write(char data) {
    while (can_transmit_empty() == 0) {}

    port_out_b(PORT, data);
}


