#ifndef __IRQ_H__
#define __IRQ_H__

#include <lib/event.h>

extern event_t irq[256];

void int_handler(void);
void irq0_handler(void);
void irq1_handler(void);
void pic0_generic(void);
void pic1_generic(void);
void apic_nmi(void);
void apic_spurious(void);

void dummy_int_handler(void);
void pit_handler(void);
void pic0_generic_handler(void);
void pic1_generic_handler(void);
void apic_nmi_handler(void);
void apic_spurious_handler(void);

void scheduler_ipi(void);

void flush_irqs(void);

void eoi(void);

#endif
