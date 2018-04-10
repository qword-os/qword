#include <idt.h>
#include <klib.h>
#include <cio.h>

void int_handler(void);

static idt_entry_t idt[256];

void init_idt(void) {
    for (size_t vec = 0; vec < 256; vec++) {
        register_interrupt_handler(vec, int_handler, 0);
    }

    idt_ptr_t idt_ptr = {
        sizeof(idt) - 1,
        #ifdef __X86_64__
            (uint64_t)idt
        #endif
        #ifdef __I386__
            (uint32_t)idt
        #endif
    };

    asm volatile("lidt %0" : : "m" (idt_ptr));
}

#ifdef __X86_64__
int register_interrupt_handler(size_t vec, void (*handler)(void), uint8_t type) {
    uint64_t p = (uint64_t)handler;

    idt[vec].offset_lo = (p & 0xffff);
    idt[vec].selector = 0x08;
    idt[vec].ist = 0;
    idt[vec].type_attr = type;
    idt[vec].offset_mid = ((p & 0xffff0000) >> 16);
    idt[vec].offset_hi = ((p & 0xffffffff00000000) >> 32);
    idt[vec].zero = 0;

    return 0;
}
#endif /* x86_64 */

#ifdef __I386__
int register_interrupt_handler(size_t vec, void (*handler)(void), uint8_t type) {
    uint32_t p = (uint32_t)handler;

    idt[vec].offset_lo = (p & 0xffff);
    idt[vec].selector = 0x08;
    idt[vec].unused = 0;
    idt[vec].type_attr = type;
    idt[vec].offset_hi = ((p & 0xffff0000) >> 16);

    return 0;
}
#endif /* i386 */

void dummy_int_handler(void) {
    kprint(KPRN_INFO, "Interrupt!");
    /* ACK this interrupt to the PICS */
    port_out_b(0x20, 0x20);
    return;
}
