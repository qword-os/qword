#include <pic.h>

void send_eoi(uint8_t current_vector) {
    if (should_use_apic) {
        /* TODO Send APIC EOI */
    }

    pic_8259_eoi(current_vector);
}

void init_pic(void) {
    check_apic();
    
    if (should_use_apic) {
        /* TODO initialise APIC, mask PIC interrupts */
    }
    
    /* FIXME: Should we make these offsets tunable? (might coincide with other vectors
    * we don't want to be tunable ...) */ 
    pic_8259_remap(0x20, 0x28);
}
