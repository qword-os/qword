#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <klib.h>

#ifdef __X86_64__
/* map physaddr -> virtaddr using pml4 pointer */
void map_page(pt_entry_t *pml4, size_t phys_addr, size_t virt_addr, size_t flags) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Check present flag */
    if (pml4[pml4_entry] & 0x1) {
        /* Reference pdpt */
        pdpt = (pt_entry_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        /* Allocate a page for the pdpt. */
        pdpt = pmm_alloc(1);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pdpt[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pml4[pml4_entry] = (pt_entry_t)pdpt | 0b111;
    }

    /* Rinse and repeat */
    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {
        /* Allocate a page for the pd. */
        pd = pmm_alloc(1);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pd[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pdpt[pdpt_entry] = (pt_entry_t)pd | 0b111;
    }

    /* Once more */
    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {
        /* Allocate a page for the pt. */
        pt = pmm_alloc(1);

        /* Zero page */
        for (size_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
            /* Zero each entry */
            pt[i] = 0;
        }

        /* Present + writable + user (0b111) */
        pd[pd_entry] = (pt_entry_t)pt | 0b111;
    }

    /* Set the entry as present and point it to the passed physical address */
    /* Also set the specified flags */
    pt[pt_entry] = (pt_entry_t)(phys_addr | flags);
    return;
}
#endif /* x86_64 */

#ifdef __X86_64__
int unmap_page(pt_entry_t *pml4, size_t virt_addr) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    /* Unmap entry */
    pt[pt_entry] = 0;

    return 0;
}
#endif /* x86_64 */

#ifdef __X86_64__
/* Update flags for a mapping */
int remap_page(pt_entry_t *pml4, size_t virt_addr, size_t flags) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pd[pd_entry] & 0x1) {
        pt = (pt_entry_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    /* Update flags */
    pt[pt_entry] = (pt[pt_entry] & 0xfffffffffffff000) | flags;

    return 0;
}
#endif /* x86_64 */

/* Identity map the first 4GiB of memory, this saves issues with MMIO hardware < 4GiB later on */
void full_identity_map(void) {
    kprint(KPRN_INFO, "VMM: Identity mapping the first 4GiB of memory...");

    for (size_t i = 0; i < (0x100000000 / PAGE_SIZE); i++) {
        size_t addr = i * PAGE_SIZE;
        map_page(&kernel_pagemap, addr, addr, 0x03);
    }

    return;
}
