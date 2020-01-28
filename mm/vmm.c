#include <stdint.h>
#include <stddef.h>
#include <mm/mm.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <sys/e820.h>
#include <lib/lock.h>
#include <sys/panic.h>
#include <lib/cmem.h>
#include <sys/cpu.h>

static struct pagemap_t __kp;
struct pagemap_t *kernel_pagemap = &__kp;

static inline size_t entries_to_virt_addr(size_t pml4_entry,
                                   size_t pdpt_entry,
                                   size_t pd_entry,
                                   size_t pt_entry) {
    size_t virt_addr = 0;

    virt_addr |= pml4_entry << 39;
    virt_addr |= pdpt_entry << 30;
    virt_addr |= pd_entry << 21;
    virt_addr |= pt_entry << 12;

    return virt_addr;
}

struct pagemap_t *new_address_space(void) {
    struct pagemap_t *new_pagemap = kalloc(sizeof(struct pagemap_t));
    if (!new_pagemap)
        return NULL;
    new_pagemap->pml4 = pmm_allocz(1);
    if (!new_pagemap->pml4) {
        kfree(new_pagemap);
        return NULL;
    }
    new_pagemap->pml4 = (void *)((size_t)new_pagemap->pml4 + MEM_PHYS_OFFSET);
    new_pagemap->lock = new_lock;
    return new_pagemap;
}

void free_address_space(struct pagemap_t *pagemap) {
    pt_entry_t *pdpt;
    pt_entry_t *pd;
    pt_entry_t *pt;

    spinlock_acquire(&pagemap->lock);

    for (size_t i = 0; i < PAGE_TABLE_ENTRIES / 2; i++) {
        if (pagemap->pml4[i] & 1) {
            pdpt = (pt_entry_t *)((pagemap->pml4[i] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
            for (size_t j = 0; j < PAGE_TABLE_ENTRIES; j++) {
                if (pdpt[j] & 1) {
                    pd = (pt_entry_t *)((pdpt[j] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
                    for (size_t k = 0; k < PAGE_TABLE_ENTRIES; k++) {
                        if (pd[k] & 1) {
                            pt = (pt_entry_t *)((pd[k] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
                            for (size_t l = 0; l < PAGE_TABLE_ENTRIES; l++) {
                                if (pt[l] & 1)
                                    pmm_free((void *)(pt[l] & 0xfffffffffffff000), 1);
                            }
                            pmm_free((void *)(pd[k] & 0xfffffffffffff000), 1);
                        }
                    }
                    pmm_free((void *)(pdpt[j] & 0xfffffffffffff000), 1);
                }
            }
            pmm_free((void *)(pagemap->pml4[i] & 0xfffffffffffff000), 1);
        }
    }

    pmm_free((void *)pagemap->pml4 - MEM_PHYS_OFFSET, 1);
    kfree(pagemap);
}

struct pagemap_t *fork_address_space(struct pagemap_t *old_pagemap) {
    /* Allocate the new pagemap */
    struct pagemap_t *new_pagemap = new_address_space();

    pt_entry_t *pdpt;
    pt_entry_t *pd;
    pt_entry_t *pt;

    struct {
        uint8_t data[PAGE_SIZE];
    } *pool;

    size_t pool_size = 4096;
    pool = pmm_alloc(pool_size);
    size_t pool_ptr = 0;

    /* Map and copy all used pages */
    for (size_t i = 0; i < PAGE_TABLE_ENTRIES / 2; i++) {
        if (old_pagemap->pml4[i] & 1) {
            pdpt = (pt_entry_t *)((old_pagemap->pml4[i] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
            for (size_t j = 0; j < PAGE_TABLE_ENTRIES; j++) {
                if (pdpt[j] & 1) {
                    pd = (pt_entry_t *)((pdpt[j] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
                    for (size_t k = 0; k < PAGE_TABLE_ENTRIES; k++) {
                        if (pd[k] & 1) {
                            pt = (pt_entry_t *)((pd[k] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
                            for (size_t l = 0; l < PAGE_TABLE_ENTRIES; l++) {
                                if (pt[l] & 1) {
                                    /* FIXME find a way to expand the pool instead of dying */
                                    if (pool_ptr == pool_size)
                                        panic("Fork memory pool exhausted", 0, 0, NULL);
                                    size_t new_page = (size_t)&pool[pool_ptr++];
                                    memcpy64((char *)(new_page + MEM_PHYS_OFFSET),
                                            (char *)((pt[l] & 0xfffffffffffff000) + MEM_PHYS_OFFSET),
                                            PAGE_SIZE);
                                    map_page(new_pagemap,
                                             new_page,
                                             entries_to_virt_addr(i, j, k, l),
                                             (pt[l] & 0xfff),0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    pmm_free(pool + pool_ptr, pool_size - pool_ptr);

    /* Map kernel into higher half */
    for (size_t i = PAGE_TABLE_ENTRIES / 2; i < PAGE_TABLE_ENTRIES; i++) {
        new_pagemap->pml4[i] = process_table[0]->pagemap->pml4[i];
    }

    return new_pagemap;
}

/* map physaddr -> virtaddr using pml4 pointer */
/* Returns 0 on success, -1 on failure */
int map_page(struct pagemap_t *pagemap, size_t phys_addr, size_t virt_addr, size_t flags, int a) {
    spinlock_acquire(&pagemap->lock);

    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Check present flag */
    if (pagemap->pml4[pml4_entry] & 0x1) {
        /* Reference pdpt */
        pdpt = (pt_entry_t *)((pagemap->pml4[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pdpt. */
        pdpt = (pt_entry_t *)((size_t)pmm_allocz(1) + MEM_PHYS_OFFSET);
        /* Catch allocation failure */
        if ((size_t)pdpt == MEM_PHYS_OFFSET)
            goto fail1;
        /* Present + writable + user (0b111) */
        pagemap->pml4[pml4_entry] = (pt_entry_t)((size_t)pdpt - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Rinse and repeat */
    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pd. */
        pd = (pt_entry_t *)((size_t)pmm_allocz(1) + MEM_PHYS_OFFSET);
        /* Catch allocation failure */
        if ((size_t)pd == MEM_PHYS_OFFSET)
            goto fail2;
        /* Present + writable + user (0b111) */
        pdpt[pdpt_entry] = (pt_entry_t)((size_t)pd - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Once more */
    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pt. */
        pt = (pt_entry_t *)((size_t)pmm_allocz(1) + MEM_PHYS_OFFSET);
        /* Catch allocation failure */
        if ((size_t)pt == MEM_PHYS_OFFSET)
            goto fail3;
        /* Present + writable + user (0b111) */
        pd[pd_entry] = (pt_entry_t)((size_t)pt - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Set the entry as present and point it to the passed physical address */
    /* Also set the specified flags */
    pt[pt_entry] = (pt_entry_t)(phys_addr | flags);

    if ((size_t)pagemap->pml4 == read_cr("3")) {
        // TODO: TLB shootdown
        invlpg(virt_addr);
    }

    spinlock_release(&pagemap->lock);
    return 0;

    /* Free previous levels if empty */
fail3:
    for (size_t i = 0; ; i++) {
        if (i == PAGE_TABLE_ENTRIES) {
            /* We reached the end, table is free */
            pmm_free((void *)pd - MEM_PHYS_OFFSET, 1);
            break;
        }
        if (pd[i] & 0x1) {
            /* Table is not free */
            goto fail1;
        }
    }

fail2:
    for (size_t i = 0; ; i++) {
        if (i == PAGE_TABLE_ENTRIES) {
            /* We reached the end, table is free */
            pmm_free((void *)pdpt - MEM_PHYS_OFFSET, 1);
            break;
        }
        if (pdpt[i] & 0x1) {
            /* Table is not free */
            goto fail1;
        }
    }

fail1:
    spinlock_release(&pagemap->lock);
    return -1;
}

int unmap_page(struct pagemap_t *pagemap, size_t virt_addr) {
    spinlock_acquire(&pagemap->lock);

    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pagemap->pml4[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)((pagemap->pml4[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    /* Unmap entry */
    pt[pt_entry] = 0;

    if ((size_t)pagemap->pml4 == read_cr("3")) {
        // TODO: TLB shootdown
        invlpg(virt_addr);
    }

    /* Free previous levels if empty */
    for (size_t i = 0; ; i++) {
        if (i == PAGE_TABLE_ENTRIES) {
            /* We reached the end, table is free */
            pmm_free((void *)pt - MEM_PHYS_OFFSET, 1);
            break;
        }
        if (pt[i] & 0x1) {
            /* Table is not free */
            goto out;
        }
    }

    for (size_t i = 0; ; i++) {
        if (i == PAGE_TABLE_ENTRIES) {
            /* We reached the end, table is free */
            pmm_free((void *)pd - MEM_PHYS_OFFSET, 1);
            break;
        }
        if (pd[i] & 0x1) {
            /* Table is not free */
            goto out;
        }
    }

    for (size_t i = 0; ; i++) {
        if (i == PAGE_TABLE_ENTRIES) {
            /* We reached the end, table is free */
            pmm_free((void *)pdpt - MEM_PHYS_OFFSET, 1);
            break;
        }
        if (pdpt[i] & 0x1) {
            /* Table is not free */
            goto out;
        }
    }

out:
    spinlock_release(&pagemap->lock);
    return 0;

fail:
    spinlock_release(&pagemap->lock);
    return -1;
}

/* Update flags for a mapping */
int remap_page(struct pagemap_t *pagemap, size_t virt_addr, size_t flags) {
    spinlock_acquire(&pagemap->lock);

    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pagemap->pml4[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)((pagemap->pml4[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        goto fail;
    }

    /* Update flags */
    pt[pt_entry] = (pt[pt_entry] & 0xfffffffffffff000) | flags;

    if ((size_t)pagemap->pml4 == read_cr("3")) {
        // TODO: TLB shootdown
        invlpg(virt_addr);
    }

    spinlock_release(&pagemap->lock);
    return 0;

fail:
    spinlock_release(&pagemap->lock);
    return -1;
}

/* Map the first 4GiB of memory, this saves issues with MMIO hardware < 4GiB later on */
/* Then use the e820 to map all the available memory (saves on allocation time and it's easier) */
/* The physical memory is mapped at the beginning of the higher half (entry 256 of the pml4) onwards */
void init_vmm(void) {
    kernel_pagemap->pml4 = (pt_entry_t *)((size_t)pmm_allocz(1) + MEM_PHYS_OFFSET);
    if ((size_t)kernel_pagemap->pml4 == MEM_PHYS_OFFSET)
        panic("init_vmm failure", 0, 0, NULL);

    spinlock_release(&kernel_pagemap->lock);

    kprint(KPRN_INFO, "vmm: Mapping memory as specified by the e820...");

    /* Identity map the first 32 MiB */
    /* Map 32 MiB for the phys mem area, and 32 MiB for the kernel in the higher half */
    for (size_t i = 0; i < (0x2000000 / PAGE_SIZE); i++) {
        size_t addr = i * PAGE_SIZE;
        map_page(kernel_pagemap, addr, addr, 0x03, 0);
        map_page(kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03, 0);
        map_page(kernel_pagemap, addr, KERNEL_PHYS_OFFSET + addr, 0x03 | (1 << 8), 0);
    }

    /* Reload new pagemap */
    write_cr("3", (size_t)kernel_pagemap->pml4 - MEM_PHYS_OFFSET);

    /* Forcefully map the first 4 GiB for I/O into the higher half */
    for (size_t i = 0; i < (0x100000000 / PAGE_SIZE); i++) {
        size_t addr = i * PAGE_SIZE;

        map_page(kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03, 0);
    }

    /* Map the rest according to e820 into the higher half */
    for (size_t i = 0; e820_map[i].type; i++) {
        size_t aligned_base = e820_map[i].base - (e820_map[i].base % PAGE_SIZE);
        size_t aligned_length = DIV_ROUNDUP(e820_map[i].length, PAGE_SIZE) * PAGE_SIZE;
        if (e820_map[i].base % PAGE_SIZE) aligned_length += PAGE_SIZE;

        for (size_t j = 0; j * PAGE_SIZE < aligned_length; j++) {
            size_t addr = aligned_base + j * PAGE_SIZE;

            /* Skip over first 4 GiB */
            if (addr < 0x100000000)
                continue;

            map_page(kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03, 0);
        }
    }

    /* lol bug fix */
    pmm_change_allocation_method();
}
