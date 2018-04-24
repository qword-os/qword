#include <stdint.h>
#include <stddef.h>
#include <gdt.h>
#include <mm.h>

void init_gdt(void) {
    size_t gdt_pages = (gdt_len + 1) / PAGE_SIZE;
    if ((gdt_len + 1) % PAGE_SIZE)
        gdt_pages++;

    for (size_t i = 0; i < gdt_pages; i++) {
        map_page(&kernel_pagemap, gdt_phys + i * PAGE_SIZE,
                        GDT_VIRT_ADDR + i * PAGE_SIZE, 0x03);
    }

    gdt_ptr_t gdt_ptr = {
        gdt_len,
        GDT_VIRT_ADDR
    };

    asm volatile (
        "lgdt %0;"
        :
        : "m" (gdt_ptr)
    );

    return;
}
