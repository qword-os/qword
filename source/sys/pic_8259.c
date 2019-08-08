#include <stdint.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <sys/pic_8259.h>

#define CMD_INIT 0x11
#define MODE_8086 0x01
#define EOI 0x20

/* EOI for PICS */
void pic_8259_eoi(uint8_t current_vector) {
    if (current_vector >= 8) {
        port_out_b(0xa0, 0x20);
    }

    port_out_b(0x20, 0x20);
}

/* Remap the PIC IRQs to the given vector offsets */
void pic_8259_remap(uint8_t pic0_offset, uint8_t pic1_offset) {
    kprint(KPRN_INFO,"pic_8259: Initialising with offsets %x and %x",
            (uint32_t)pic0_offset,
            (uint32_t)pic1_offset);

    /* Save masks */
    uint8_t pic0_mask = port_in_b(0x21);
    uint8_t pic1_mask = port_in_b(0xa1);

    port_out_b(0x20, CMD_INIT);
    io_wait();
    port_out_b(0xa0, CMD_INIT);
    io_wait();

    port_out_b(0x21, pic0_offset);
    io_wait();
    port_out_b(0xa1, pic1_offset);
    io_wait();

    port_out_b(0x21, 4);
    io_wait();
    port_out_b(0xa1, 2);
    io_wait();

    port_out_b(0x21, MODE_8086);
    io_wait();
    port_out_b(0xa1, MODE_8086);
    io_wait();

    /* Restore masks */
    port_out_b(0x21, pic0_mask);
    io_wait();
    port_out_b(0xa1, pic1_mask);
    io_wait();

    return;
}

/* Mask IRQ `line`. */
void pic_8259_set_mask(uint8_t line, int status) {
    uint16_t port;
    uint8_t value;

    if (line < 8) {
        port = 0x21;
    } else {
        port = 0xa1;
        line -= 8;
    }

    if (!status)
        value = port_in_b(port) & ~((uint8_t)1 << line);
    else
        value = port_in_b(port) | ((uint8_t)1 << line);

    port_out_b(port, value);
    io_wait();

    return;
}

void pic_8259_mask_all(void) {
    port_out_b(0xa1, 0xff);
    port_out_b(0x21, 0xff);
}
