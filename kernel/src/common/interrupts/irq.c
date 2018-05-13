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

void dummy_int_handler(void) {
    kprint(KPRN_INFO, "Unhandled interrupt occurred");

    return;
}

/* Interrupts should be OFF */
void pit_handler(ctx_t *prev, uint64_t *pagemap) {
    if (!(++uptime_raw % PIT_FREQUENCY)) {
        uptime_sec++;
    }

    pic_send_eoi(0);

    /* This function will release the lock after updating the process table */
    task_resched(prev, pagemap);

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
