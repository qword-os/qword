#include <irq.h>
#include <pic.h>
#include <apic.h>
#include <pic_8259.h>
#include <klib.h>
#include <time.h>
#include <pit.h>

void pit_handler(void) {
    if (!(++uptime_raw % PIT_FREQUENCY)) {
        uptime_sec++;
    }
    pic_send_eoi(0);
    return;
}

void pic_generic_handler(void) {
    /* Hacky, but it ensures both of the PICs are EOI'd, hence all eventualities are covered. */
    pic_8259_eoi(8); 
    return;
}

void apic_nmi_handler(void) {
    kprint(KPRN_WARN, "Warning: Non-maskable interrupt occured. Possible hardware issue...");
    lapic_eoi();
    return;
}

void apic_spurious_handler(void) {
    lapic_eoi();
    return;
}
