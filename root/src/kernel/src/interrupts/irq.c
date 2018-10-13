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
#include <panic.h>

/* Interrupts should be OFF */
void pit_handler(void) {
    if (!(++uptime_raw % PIT_FREQUENCY)) {
        uptime_sec++;
    }

    return;
}

void pic0_generic_handler(void) {
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0);
}

void pic1_generic_handler(void) {
    port_out_b(0xa0, 0x20);
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0);
}

void apic_nmi_handler(void) {
    lapic_eoi();
    panic("apic: Non-maskable interrupt occured. Possible hardware issue...", 0, 0);
}

void apic_spurious_handler(void) {
    lapic_eoi();
    panic("apic: Spurious interrupt occurred.", 0, 0);
}

void dummy_int_handler(void) {
    panic("kernel: Unhandled interrupt occurred.", 0, 0);
}
