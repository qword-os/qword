#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#ifdef __X86_64__
    #define PAGE_TABLE_ENTRIES 512
    #define KERNEL_PHYS_OFFSET ((size_t)0xffffffffc0000000)
#endif
#ifdef __I386__
    #define PAGE_TABLE_ENTRIES 1024
    #define KERNEL_PHYS_OFFSET ((size_t)0xa0000000)
#endif

#ifdef __X86_64__
    typedef uint64_t pt_entry_t;
#endif
#ifdef __I386__
    typedef uint32_t pt_entry_t;
#endif

extern pt_entry_t kernel_pagemap;

void *pmm_alloc(size_t);
void pmm_free(void *, size_t);
void init_pmm(void);

void map_page(pt_entry_t *, size_t, size_t, size_t);
int unmap_page(pt_entry_t *, size_t);
int remap_page(pt_entry_t *, size_t, size_t);
void init_vmm(void);

#endif
