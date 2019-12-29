#ifndef __SYS__ISRS_H__
#define __SYS__ISRS_H__

#include <sys/panic.h>
#include <sys/idt.h>
#include <lib/cio.h>
#include <lib/time.h>
#include <lib/event.h>

extern void irq0_handler(void *);

__attribute__((interrupt)) static void pic0_generic_handler(void *p) {
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0, NULL);
}

__attribute__((interrupt)) static void pic1_generic_handler(void *p) {
    port_out_b(0xa0, 0x20);
    port_out_b(0x20, 0x20);
    panic("pic_8259: Spurious interrupt occured.", 0, 0, NULL);
}

__attribute__((interrupt)) static void apic_nmi_handler(void *p) {
    lapic_eoi();
    panic("apic: Non-maskable interrupt occured. Possible hardware issue...", 0, 0, NULL);
}

__attribute__((interrupt)) static void apic_spurious_handler(void *p) {
    lapic_eoi();
    panic("apic: Spurious interrupt occurred.", 0, 0, NULL);
}

#endif
