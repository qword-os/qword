#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>
#include <stdint.h>

void irq0_handler(void);
void pic_generic(void);
void apic_nmi(void);
void apic_spurious(void);

void pit_handler(void);
void pic_generic_handler(void);
void apic_nmi_handler(void);
void apic_spurious_handler(void);

#endif
