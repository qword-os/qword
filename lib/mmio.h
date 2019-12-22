#ifndef __MMIO_H__
#define __MMIO_H__

#include <stdint.h>

uint8_t mmio_read8(uint64_t address);
uint16_t mmio_read16(uint64_t address);
uint32_t mmio_read32(uint64_t address);
uint64_t mmio_read64(uint64_t address);

void mmio_write8(uint64_t address, uint8_t value);
void mmio_write16(uint64_t address, uint16_t value);
void mmio_write32(uint64_t address, uint32_t value);
void mmio_write64(uint64_t address, uint64_t value);

#endif
