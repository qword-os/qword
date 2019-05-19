#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>
#include <stddef.h>
// TODO regs_t should be in sys/cpu.h
#include <proc/task.h>

struct idt_entry_t {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
};

struct idt_ptr_t {
    uint16_t size;
    /* Start address */
    uint64_t address;
} __attribute((packed));

void init_idt(void);
int register_interrupt_handler(size_t, void (*)(void), uint8_t, uint8_t);
int register_isr(size_t, void (**)(int, struct regs_t *), size_t, uint8_t, uint8_t);

extern void *isr_handler_addresses[];
extern void **isr_function_addresses[];

#endif
