#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <klib.h>
#include <e820.h>

struct pagemap kernel_pagemap;

/* map physaddr -> virtaddr using pml4 pointer */
void map_page(struct pagemap *pml4, size_t phys_addr, size_t virt_addr, size_t flags) {
    spinlock_acquire(&pml4->lock);
    
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Check present flag */
    if (pml4->pagemap[pml4_entry] & 0x1) {
        /* Reference pdpt */
        pdpt = (pt_entry_t *)((pml4->pagemap[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pdpt. */
        pdpt = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pdpt[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pml4->pagemap[pml4_entry] = (pt_entry_t)((size_t)pdpt - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Rinse and repeat */
    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pd. */
        pd = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pd[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pdpt[pdpt_entry] = (pt_entry_t)((size_t)pd - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Once more */
    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        /* Allocate a page for the pt. */
        pt = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pt[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pd[pd_entry] = (pt_entry_t)((size_t)pt - MEM_PHYS_OFFSET) | 0b111;
    }

    /* Set the entry as present and point it to the passed physical address */
    /* Also set the specified flags */
    pt[pt_entry] = (pt_entry_t)(phys_addr | flags);
    spinlock_release(&pml4->lock);
    return;
}

int unmap_page(struct pagemap *pml4, size_t virt_addr) {
    spinlock_acquire(&pml4->lock);
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4->pagemap[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)((pml4->pagemap[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    /* Unmap entry */
    pt[pt_entry] = 0;
    
    spinlock_release(&pml4->lock);

    return 0;
}

/* Update flags for a mapping */
int remap_page(struct pagemap *pml4, size_t virt_addr, size_t flags) {
    spinlock_acquire(&pml4->lock);

    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4->pagemap[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)((pml4->pagemap[pml4_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_entry] & 0xfffffffffffff000) + MEM_PHYS_OFFSET);
    } else {
        return -1;
    }

    /* Update flags */
    pt[pt_entry] = (pt[pt_entry] & 0xfffffffffffff000) | flags;

    spinlock_release(&pml4->lock);

    return 0;
}

/* Map the first 4GiB of memory, this saves issues with MMIO hardware < 4GiB later on */
/* Then use the e820 to map all the available memory (saves on allocation time and it's easier) */
/* The physical memory is mapped at the beginning of the higher half (entry 256 of the pml4) onwards */
void init_vmm(void) {
    kernel_pagemap.pagemap = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);
    kernel_pagemap.lock = 1;

    for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
        kernel_pagemap.pagemap[i] = 0;

    kprint(KPRN_INFO, "vmm: Mapping memory as specified by the e820...");

    /* Identity map the first 32 MiB */
    /* Map 32 MiB for the phys mem area, and 32 MiB for the kernel in the higher half */
    for (size_t i = 0; i < (0x2000000 / PAGE_SIZE); i++) {
        size_t addr = i * PAGE_SIZE;
        map_page(&kernel_pagemap, addr, addr, 0x03);
        map_page(&kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03);
        map_page(&kernel_pagemap, addr, KERNEL_PHYS_OFFSET + addr, 0x03);
    }

    /* Reload new pagemap */
    asm volatile (
        "mov cr3, rax;"
        :
        : "a" ((size_t)kernel_pagemap.pagemap - MEM_PHYS_OFFSET)
    );

    /* Forcefully map the first 4 GiB for I/O into the higher half */
    for (size_t i = 0; i < (0x100000000 / PAGE_SIZE); i++) {
        size_t addr = i * PAGE_SIZE;

        map_page(&kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03);
    }

    /* Map the rest according to e820 into the higher half */
    for (size_t i = 0; e820_map[i].type; i++) {
        for (size_t j = 0; j * PAGE_SIZE < e820_map[i].length; j++) {
            size_t addr = e820_map[i].base + j * PAGE_SIZE;

            /* Skip over first 4 GiB */
            if (addr < 0x100000000)
                continue;

            map_page(&kernel_pagemap, addr, MEM_PHYS_OFFSET + addr, 0x03);
        }
    }

    return;
}
