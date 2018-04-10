#include <cio.h>
#include <klib.h>
#include <pic.h>

#define CMD_INIT 0x11
#define MODE_8086 0x01
#define WAIT_PORT 0x80
#define EOI 0x20

/* Remap the PIC IRQs to the given vector offsets */
void remap_pic(uint8_t pic0_offset, uint8_t pic1_offset) {
    kprint(KPRN_INFO,"pic_8259: Initialising with offsets %X and %X",
            pic0_offset,
            pic1_offset);

    port_out_b(0x20, CMD_INIT);
    wait();
    port_out_b(0xA0, CMD_INIT);
    wait();

    port_out_b(0x21, pic0_offset);
    wait();
    port_out_b(0xA1, pic1_offset);
    wait();

    port_out_b(0x21, 4);
    wait();
    port_out_b(0xA1, 2);
    wait();

    port_out_b(0x21, MODE_8086);
    wait();
    port_out_b(0xA1, MODE_8086);
    wait();

    /* Clear all masks, enabling all IRQs. */
    for (uint8_t line = 0; line < 16; line++) {
        clear_mask(line);
    }
}

/* On older, hardware, commands sent to the PIC may take a while. Writing to I/O port 80h
 * takes long enough to provide a simple wait mechanism */
void wait(void) {
    port_out_b(WAIT_PORT, 0);
}

void clear_mask(uint8_t line) {
    uint16_t port;
    uint8_t value;

    if (line < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        line -= 8;
    }
    
    value = port_in_b(port) & ~(1 << line);
    port_out_b(port, value);
}
