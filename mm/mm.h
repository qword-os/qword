#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/ht.h>
#include <startup/stivale.h>

#define PAGE_SIZE ((size_t)4096)

#define PAGE_TABLE_ENTRIES 512
#define KERNEL_PHYS_OFFSET ((size_t)0xffffffff80000000)
#define MEM_PHYS_OFFSET ((size_t)0xffff800000000000)

typedef uint64_t pt_entry_t;

struct page_attributes_t {
    char name[18];
    int attr;
    int refcount;
    size_t phys_addr;
    size_t virt_addr;
    size_t flags;
};

struct pagemap_t {
    ht_new(struct page_attributes_t, page_attributes);
    pt_entry_t *pml4;
    lock_t lock;
};

extern struct pagemap_t *kernel_pagemap;

void pmm_change_allocation_method(void);

extern void *(*pmm_alloc)(size_t);
void *pmm_allocz(size_t);
void pmm_free(void *, size_t);
void init_pmm(struct stivale_memmap_t *);

int map_page(struct pagemap_t *, size_t, size_t, size_t);
int unmap_page(struct pagemap_t *, size_t);
int remap_page(struct pagemap_t *, size_t, size_t);
void init_vmm(struct stivale_memmap_t *);

struct pagemap_t *new_address_space(void);
struct pagemap_t *fork_address_space(struct pagemap_t *);
void free_address_space(struct pagemap_t *);

struct memstats {
    size_t total;
    size_t used;
};

int getmemstats(struct memstats *);

#endif
