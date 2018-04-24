#ifndef __GDT_H__
#define __GDT_H__

#ifdef __X86_64__
    typedef struct {
        uint16_t length;
        uint64_t addr;
    } __attribute__((packed)) gdt_ptr_t;

    #define GDT_VIRT_ADDR 0xffffffff80000000
#endif

extern uint16_t gdt_len;
extern size_t gdt_phys;

void init_gdt(void);

#endif
