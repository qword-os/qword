#include <idt.h>
#include <klib.h>

extern void int_handler(void);
    
static idt_entry_t idt[256];

void init_idt(void) {
    for (size_t vec = 0; vec < 256; vec++) {
        register_interrupt_handler(0, vec, int_handler, 0x8F);
    }
    
    idt_ptr_t idt_ptr = {
        sizeof(idt) - 1,
        (uint64_t)idt
    };

    asm volatile("lidt %0" : : "m"(idt_ptr));
}

int register_interrupt_handler(uint8_t ist_idx, size_t vec, void (handler)(void), uint8_t type) {
    uint64_t p = (uint64_t)handler;
    
    idt[vec].offset_lo = (p & 0xffff);
    idt[vec].selector = 0x08;
    idt[vec].ist = ist_idx;
    idt[vec].type_attr = type;
    idt[vec].offset_mid = ((p & 0xFFFF0000) >> 16);
    idt[vec].offset_hi = ((p & 0xFFFFFFFF00000000) >> 32);
    idt[vec].zero = 0;

    return 0;
}

void dummy_int_handler(void) {
    kprint(KPRN_INFO, "Interrupt!");
}
