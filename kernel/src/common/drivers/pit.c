#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <pit.h>
#include <pic.h>

void init_pit(void) {
    kprint(KPRN_INFO, "pit: Setting frequency to %uHz", (uint64_t)PIT_FREQUENCY);

    uint16_t x = 1193182 / PIT_FREQUENCY;
    if ((1193182 % PIT_FREQUENCY) > (PIT_FREQUENCY / 2))
        x++;
        
    port_out_b(0x40, (uint8_t)(x & 0x00ff));
    port_out_b(0x40, (uint8_t)((x & 0xff00) >> 8));

    kprint(KPRN_INFO, "pit: Frequency updated");

    kprint(KPRN_INFO, "pit: Unmasking PIT IRQ");
    pic_set_mask(0, 1);

    return;
}
