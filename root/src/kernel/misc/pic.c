#include <stdint.h>
#include <apic.h>
#include <pic_8259.h>
#include <pic.h>
#include <panic.h>

void init_pic(void) {
    if (apic_supported()) {
        pic_8259_remap(0xa0, 0xa8);
        pic_8259_mask_all();
        init_apic();
    } else {
        panic("APIC not available", 0, 0);
    }

    return;
}
