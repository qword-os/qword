#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>

static inline uint8_t mmio_read8(uint64_t address) {
    return *((volatile uint8_t*)(address));
}

static inline uint16_t mmio_read16(uint64_t address) {
    return *((volatile uint16_t*)(address));
}

static inline uint32_t mmio_read32(uint64_t address) {
    return *((volatile uint32_t*)(address));
}

static inline uint64_t mmio_read64(uint64_t address) {
    return *((volatile uint64_t*)(address));
}

static inline void mmio_write8(uint64_t address, uint8_t value) {
    (*((volatile uint8_t*)(address))) = value;
}

static inline void mmio_write16(uint64_t address, uint16_t value) {
    (*((volatile uint16_t*)(address))) = value;
}

static inline void mmio_write32(uint64_t address, uint32_t value) {
    (*((volatile uint32_t*)(address))) = value;
}

static inline void mmio_write64(uint64_t address, uint64_t value) {
    (*((volatile uint64_t*)(address)))= value;
}

#define port_out_b(port, value) ({				\
	asm volatile (	"out dx, al"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_out_w(port, value) ({				\
	asm volatile (	"out dx, ax"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_out_d(port, value) ({				\
	asm volatile (	"out dx, eax"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_in_b(port) ({						\
	uint8_t value;								\
	asm volatile (	"in al, dx"					\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#define port_in_w(port) ({						\
	uint16_t value;								\
	asm volatile (	"in ax, dx"					\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#define port_in_d(port) ({						\
	uint32_t value;								\
	asm volatile (	"in eax, dx"				\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#define io_wait() ({ port_out_b(0x80, 0x00); })

#define disable_interrupts() ({ asm volatile ("cli"); })
#define enable_interrupts() ({ asm volatile ("sti"); })

#endif
