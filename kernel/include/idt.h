#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __X86_64__
    typedef struct idt_entry_t {
        uint16_t offset_lo; 
        uint16_t selector; 
        uint8_t ist;       
        uint8_t type_attr;
        uint16_t offset_mid;
        uint32_t offset_hi;
        uint32_t zero;
    } idt_entry_t;
#endif
#ifdef __I386__
    typedef struct idt_entry_t {
        uint16_t offset_lo;
        uint16_t selector;
        uint8_t unused;
        uint8_t type_attr;
        uint16_t offset_hi;
    } idt_entry_t;
#endif

#ifdef __X86_64__
    typedef struct idt_ptr_t {
        uint16_t size;
        /* Start address */
        uint64_t address; 
    } __attribute__((packed)) idt_ptr_t;
#endif
#ifdef __I386__
    typedef struct idt_ptr_t {
        uint16_t size;
        /* Start address */
        uint32_t address; 
    } __attribute__((packed)) idt_ptr_t;
#endif

void init_idt(void);
int register_interrupt_handler(size_t, void (*)(void), uint8_t);
void dummy_int_handler(void);

#endif
