#include <sys/gdt.h>
#include <stdint.h>

struct gdt_entry {
    uint16_t limit;
    uint16_t base_low16;
    uint8_t  base_mid8;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high8;
} __attribute__((packed));

struct tss_entry {
    uint16_t length;
    uint16_t base_low16;
    uint8_t  base_mid8;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high8;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed));

struct gdt {
    struct gdt_entry entries[5];
    struct tss_entry tss;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t size;
    uint64_t address;
} __attribute__((packed));

static struct gdt         gdt;
static struct gdt_pointer gdt_pointer;

void init_gdt(void) {
    // Null descriptor.
    gdt.entries[0].limit       = 0;
    gdt.entries[0].base_low16  = 0;
    gdt.entries[0].base_mid8   = 0;
    gdt.entries[0].access      = 0;
    gdt.entries[0].granularity = 0;
    gdt.entries[0].base_high8  = 0;

    // Kernel code 64.
    gdt.entries[1].limit       = 0;
    gdt.entries[1].base_low16  = 0;
    gdt.entries[1].base_mid8   = 0;
    gdt.entries[1].access      = 0b10011010;
    gdt.entries[1].granularity = 0b00100000;
    gdt.entries[1].base_high8  = 0;

    // Kernel data 64.
    gdt.entries[2].limit       = 0;
    gdt.entries[2].base_low16  = 0;
    gdt.entries[2].base_mid8   = 0;
    gdt.entries[2].access      = 0b10010010;
    gdt.entries[2].granularity = 0;
    gdt.entries[2].base_high8  = 0;

    // User data 64.
    gdt.entries[3].limit       = 0;
    gdt.entries[3].base_low16  = 0;
    gdt.entries[3].base_mid8   = 0;
    gdt.entries[3].access      = 0b11110010;
    gdt.entries[3].granularity = 0;
    gdt.entries[3].base_high8  = 0;

    // User code 64.
    gdt.entries[4].limit       = 0;
    gdt.entries[4].base_low16  = 0;
    gdt.entries[4].base_mid8   = 0;
    gdt.entries[4].access      = 0b11111010;
    gdt.entries[4].granularity = 0;
    gdt.entries[4].base_high8  = 0;

    // TSS.
    gdt.tss.length       = 104;
    gdt.tss.base_low16   = 0;
    gdt.tss.base_mid8    = 0;
    gdt.tss.flags1       = 0b10001001;
    gdt.tss.flags2       = 0;
    gdt.tss.base_high8   = 0;
    gdt.tss.base_upper32 = 0;
    gdt.tss.reserved     = 0;

    // Set the pointer.
    gdt_pointer.size    = sizeof(gdt) - 1;
    gdt_pointer.address = (uint64_t)&gdt;
    asm volatile (
        "lgdt %0\n\t"
        "push rbp\n\t"
        "mov rbp, rsp\n\t"
        "push %1\n\t"
        "push rbp\n\t"
        "pushfq\n\t"
        "push %2\n\t"
        "push OFFSET 1f\n\t"
        "iretq\n\t"
        "1:\n\t"
        "pop rbp\n\t"
        "mov ds, %1\n\t"
        "mov es, %1\n\t"
        "mov fs, %1\n\t"
        "mov gs, %1\n\t"
        "mov ss, %1\n\t"
        :
        : "m"(gdt_pointer), "r"((uint64_t)0x10), "r"((uint64_t)0x08)
        : "memory"
    );
}

void load_tss(size_t addr) {
    gdt.tss.base_low16   = (uint16_t)addr;
    gdt.tss.base_mid8    = (uint8_t)(addr >> 16);
    gdt.tss.flags1       = 0b10001001;
    gdt.tss.flags2       = 0;
    gdt.tss.base_high8   = (uint8_t)(addr >> 24);
    gdt.tss.base_upper32 = (uint32_t)(addr >> 32);
    gdt.tss.reserved     = 0;
}
