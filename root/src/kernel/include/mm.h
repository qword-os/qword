#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>
#include <lock.h>

#define PAGE_SIZE ((size_t)4096)

#define PAGE_TABLE_ENTRIES 512
#define KERNEL_PHYS_OFFSET ((size_t)0xffffffffc0000000)
#define MEM_PHYS_OFFSET ((size_t)0xffff800000000000)

typedef uint64_t pt_entry_t;

struct pagemap_t {
    pt_entry_t *pml4;
    lock_t lock;
};

extern struct pagemap_t kernel_pagemap;
extern pt_entry_t kernel_cr3;

void *pmm_alloc(size_t);
void pmm_free(void *, size_t);
void init_pmm(void);

int map_page(struct pagemap_t *, size_t, size_t, size_t);
int unmap_page(struct pagemap_t *, size_t);
int remap_page(struct pagemap_t *, size_t, size_t);
void init_vmm(void);

struct pagemap_t *new_address_space(void);
struct pagemap_t *fork_address_space(struct pagemap_t *);
void free_address_space(struct pagemap_t *);

#define invlpg(addr) ({ \
    asm volatile ( \
        "invlpg [rbx];" \
        : \
        : "b" (addr) \
    ); \
})

#define load_cr3(NEW_CR3) ({ \
    asm volatile ("mov cr3, rax;" : : "a" (NEW_CR3)); \
})

#define read_cr3() ({ \
    size_t cr3; \
    asm volatile ("mov rax, cr3;" : "=a" (cr3)); \
    cr3; \
})

#define read_cr2() ({ \
    size_t cr2; \
    asm volatile ("mov rax, cr2;" : "=a" (cr2)); \
    cr2; \
})

#endif
