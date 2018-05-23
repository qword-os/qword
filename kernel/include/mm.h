#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>
#include <lock.h>

#define PAGE_SIZE 4096

#define PAGE_TABLE_ENTRIES 512
#define KERNEL_PHYS_OFFSET ((size_t)0xffffffffc0000000)
#define MEM_PHYS_OFFSET ((size_t)0xffff800000000000)

typedef uint64_t pt_entry_t;

typedef struct {
    pt_entry_t *pagemap;
    lock_t lock;
} pagemap_t;

extern pagemap_t kernel_pagemap;
extern pt_entry_t kernel_cr3;

void *pmm_alloc(size_t);
void pmm_free(void *, size_t);
void init_pmm(void);

void map_page(pagemap_t *, size_t, size_t, size_t);
int unmap_page(pagemap_t *, size_t);
int remap_page(pagemap_t *, size_t, size_t);
void init_vmm(void);

#endif
