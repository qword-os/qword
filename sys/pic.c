#include <stdint.h>
#include <sys/apic.h>
#include <sys/pic_8259.h>
#include <sys/pic.h>
#include <sys/panic.h>

void init_pic(void) {
    if (apic_supported()) {
        pic_8259_remap(0xa0, 0xa8);
        pic_8259_mask_all();
        init_apic();
    } else {
        panic("APIC not available", 0, 0, NULL);
    }

    return;
}
