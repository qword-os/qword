#ifndef __APIC_H__
#define __APIC_H__

#include <stdint.h>
#include <stddef.h>

#define APICREG_ICR0 0x300
#define APICREG_ICR1 0x310

#define IPI_BASE 0x40
#define IPI_RESCHED (IPI_BASE + 1)
#define IPI_ABORT (IPI_BASE + 0)

int apic_supported(void);

uint32_t lapic_read(uint32_t);
void lapic_write(uint32_t, uint32_t);
void lapic_set_nmi(uint8_t, uint16_t, uint8_t);
void lapic_enable(void);
void lapic_eoi(void);
void lapic_send_ipi(int, uint8_t);

uint32_t io_apic_read(size_t, uint32_t);
void io_apic_write(size_t, uint32_t, uint32_t);
size_t io_apic_from_redirect(uint32_t);
uint32_t io_apic_get_max_redirect(size_t);
void io_apic_set_redirect(uint8_t, uint32_t, uint16_t, uint8_t, int);
void io_apic_set_mask(int, int, int);

void init_apic(void);

#endif
