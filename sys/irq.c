#include <sys/irq.h>
#include <sys/pic.h>
#include <sys/apic.h>
#include <acpi/madt.h>
#include <sys/pic_8259.h>
#include <lib/klib.h>
#include <lib/time.h>
#include <sys/hpet.h>
#include <lib/cio.h>
#include <proc/task.h>
#include <sys/smp.h>
#include <sys/panic.h>

/* Interrupts should be OFF */
void pit_handler(void) {
    if (!(++uptime_raw % HPET_FREQUENCY_HZ)) {
        uptime_sec++;
        unix_epoch++;
    }
}

void pic0_generic_handler(void) {
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0, NULL);
}

void pic1_generic_handler(void) {
    port_out_b(0xa0, 0x20);
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0, NULL);
}

void apic_nmi_handler(void) {
    lapic_eoi();
    panic("apic: Non-maskable interrupt occured. Possible hardware issue...", 0, 0, NULL);
}

void apic_spurious_handler(void) {
    lapic_eoi();
    panic("apic: Spurious interrupt occurred.", 0, 0, NULL);
}

void dummy_int_handler(void) {
    panic("kernel: Unhandled interrupt occurred.", 0, 0, NULL);
}
