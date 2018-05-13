#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>
#include <ctx.h>

void int_handler(void);
void irq0_handler(void);
void pic0_generic(void);
void pic1_generic(void);
void apic_nmi(void);
void apic_spurious(void);

void dummy_int_handler(void);
void pit_handler(ctx_t *, uint64_t *);
void pic0_generic_handler(void);
void pic1_generic_handler(void);
void apic_nmi_handler(void);
void apic_spurious_handler(void);

#endif
