#include <mm.h>
#include <klib.h>

/* map physaddr -> virtaddr using pml4 pointer 
 * TODO: add typedef entry_t to uint64_t. */
void map_page(uint64_t *pml4, uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff) << 12) >> 12;

    uint64_t *pdpt, *pd, *pt;

    /* Check present flag */
    if (pml4[pml4_entry] & 0x1) {
        /* Reference pdpt */
        pdpt = (uint64_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        /* Allocate a page for the pdpt. */
        pdpt = pmm_alloc(1);
        
        /* Zero page */
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            /* Zero each byte */
            ((char *)pdpt)[i] = 0;
        }

        pml4[pml4_entry] = (uint64_t)pdpt;
        /* Present + writable (0b11) */
        pml4[pml4_entry] |= 0x3;
    }
    
    /* Rinse and repeat */
    if (pdpt[pdpt_entry] & 0x1) {
        pd = (uint64_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {    
        /* Allocate a page for the pd. */
        pd = pmm_alloc(1);
        
        /* Zero page */
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            ((char *)pd)[i] = 0;
        }

        pdpt[pdpt_entry] = (uint64_t)pd;
        /* Present + writable (0b11) */
        pdpt[pdpt_entry] |= 0x3; 
    }

    if (pd[pd_entry] & 0x1) {
        pt = (uint64_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {    
        /* Allocate a page for the pt. */
        pt = pmm_alloc(1);
        
        /* Zero page */
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            ((char *)pt)[i] = 0;
        }

        pd[pd_entry] = (uint64_t)pt;
        /* Present + writable (0b11) */
        pd[pd_entry] |= 0x3; 
    }
    
    /* Set the entry as present and point it to the passed physical address */
    pt[pt_entry] = (uint64_t)(phys_addr | flags);
    return;
}

int unmap_page(uint64_t *pml4, uint64_t virt_addr) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff) << 12) >> 12;

    uint64_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4[pml4_entry] & 0x1) {
        pdpt = (uint64_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (uint64_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }
    
    if (pd[pd_entry] & 0x1) {
        pt =(uint64_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    /* Unset all previous flags */
    pt[pt_entry] = (uint64_t)0;

    return 0;
}

/* Update flags for a mapping */
int remap_page(uint64_t *pml4, uint64_t virt_addr, uint64_t flags) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_entry = (virt_addr & ((size_t)0x1ff << 39)) >> 39;
    size_t pdpt_entry = (virt_addr & ((size_t)0x1ff << 30)) >> 30;
    size_t pd_entry = (virt_addr & ((size_t)0x1ff << 21)) >> 21;
    size_t pt_entry = (virt_addr & ((size_t)0x1ff << 12)) >> 12;

    uint64_t *pdpt, *pd, *pt;

    /* Get reference to the various tables in sequence. Return -1 if one of the tables is not present,
     * since we cannot unmap a virtual address if we don't know what it's mapped to in the first place */
    if (pml4[pml4_entry] & 0x1) {
        pdpt = (uint64_t *)(pml4[pml4_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    if (pdpt[pdpt_entry] & 0x1) {
        pd = (uint64_t *)(pdpt[pdpt_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }
    
    if (pd[pd_entry] & 0x1) {
        pt =(uint64_t *)(pd[pd_entry] & 0xfffffffffffff000);
    } else {
        return -1;
    }

    /* Update flags */
    pt[pt_entry] = (pt[pt_entry]) | flags;

    return 0;
}

/* Identity map the first 4GiB of memory, this saves issues with MMIO hardware < 4GiB later on */
void full_identity_map(void) {
    kprint(KPRN_INFO, "Identity mapping the first 4GiB of memory...");

    for (size_t i = 0; i * PAGE_SIZE < 0x100000000; i++) {
        uint64_t addr = i * PAGE_SIZE;
        map_page(kernel_pagemap, addr, addr, 0x03);
    }

    return;
}
