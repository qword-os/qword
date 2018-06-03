#include <irq.h>
#include <pic.h>
#include <apic.h>
#include <acpi/madt.h>
#include <pic_8259.h>
#include <klib.h>
#include <time.h>
#include <pit.h>
#include <cio.h>
#include <task.h>
#include <smp.h>

void dummy_int_handler(void) {
    kprint(KPRN_INFO, "Unhandled interrupt occurred");

    return;
}

/* Interrupts should be OFF */
void pit_handler(void) {    
    if (!(++uptime_raw % PIT_FREQUENCY)) {
        uptime_sec++;
    }

    return;
}

void pic0_generic_handler(void) {
    port_out_b(0x20, 0x20);
    return;
}

void pic1_generic_handler(void) {
    port_out_b(0xa0, 0x20);
    port_out_b(0x20, 0x20);
    return;
}

void apic_nmi_handler(void) {
    kprint(KPRN_WARN, "apic: Non-maskable interrupt occured. Possible hardware issue...");
    lapic_eoi();
    return;
}

void apic_spurious_handler(void) {
    lapic_eoi();
    return;
}
