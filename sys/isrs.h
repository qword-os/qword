#ifndef __SYS__ISRS_H__
#define __SYS__ISRS_H__

#include <sys/idt.h>
#include <sys/apic.h>
#include <lib/cio.h>
#include <lib/event.h>
#include <lib/klib.h>

extern void irq0_handler(void);

__attribute__((interrupt)) static void pic0_generic_handler(void *p) {
    (void)p;
    port_out_b(0x20, 0x20);
    kprint(KPRN_WARN, "pic_8259: Spurious interrupt occured.");
}

__attribute__((interrupt)) static void pic1_generic_handler(void *p) {
    (void)p;
    port_out_b(0xa0, 0x20);
    port_out_b(0x20, 0x20);
    kprint(KPRN_WARN, "pic_8259: Spurious interrupt occured.");
}

__attribute__((interrupt)) static void apic_nmi_handler(void *p) {
    (void)p;
    lapic_eoi();
    kprint(KPRN_WARN, "apic: Non-maskable interrupt occured. Possible hardware issue...");
}

__attribute__((interrupt)) static void apic_spurious_handler(void *p) {
    (void)p;
    lapic_eoi();
    kprint(KPRN_WARN, "apic: Spurious interrupt occurred.");
}

#endif
