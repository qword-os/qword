#include <elf.h>
#include <klib.h>
#include <task.h>
#include <fs.h>
#include <panic.h>

/* Execute an ELF file given the file path */
int elf_exec(char *path) {
    char *elf_magic = "\177ELF";
    stat_t *statbuf;

    int handle = vfs_open(path, 0, 0);
    if (handle == -1) return -1;

    /* Get data on the size and etc. of the file */
    if ((statbuf = kalloc(sizeof(stat_t))) == 0) return -1;
    vfs_fstat(handle, statbuf);

    /* Allocate space for the file as described by the stat_t,
     * and then read the file into
     * the buffer we just allocated */
    uint8_t *file;

    if ((file = kalloc(statbuf->sz)) == 0) return -1;

    int sts = vfs_read(handle, file, statbuf->sz);
    kfree(statbuf);

    /* Now do actual elf stuff */
    elf_hdr_t *elf_hdr = (elf_hdr_t *)file;

    /* Check the magic number is good; if not, this is not a valid
     * ELF file */
    for (size_t i = 0; i < 4; i++) {
        if (elf_hdr->ident[i] != elf_magic[i]) {
            return -1;
        } else {
            continue;
        }
    }

    /* Check if 64-bit */
    if (elf_hdr->ident[EI_CLASS] != 0x02) return -1;
    /* LE */
    if (elf_hdr->ident[EI_DATA] != 0x01) return -1;
    if (elf_hdr->ident[EI_OSABI] != ABI_SYSV) return -1;
    if (elf_hdr->machine != ARCH_x86_64) return -1;

    /* Reference the program table */
    elf_phdr_t *elf_phdr = (elf_phdr_t *)((char *)elf_hdr + elf_hdr->phoff);

    pt_entry_t *pml4 = kalloc(0x1000);
    if (!pml4) panic("Out of memory!", 0, 0);
    pagemap_t new_pagemap = {pml4, 1};

    for (size_t i = 0; i < elf_hdr->ph_num; i++) {
        if (elf_phdr[i].p_type != PT_LOAD)
            continue;

        void *phys;

        while (elf_phdr[i].p_memsz % 0x1000) elf_phdr[i].p_memsz++;

        size_t j;
        for (j = 0; j < elf_phdr[i].p_memsz / 0x1000; j++) {
            if ((phys = kalloc(0x1000)) == 0) return -1;
            size_t virt = elf_phdr[i].p_vaddr + (j * 0x1000);
            /* Map into target pagemap */
            map_page(&new_pagemap, (size_t)phys, virt, 0x07);
        }

        /* Copy to base address */
        void *init_phys = (void *)((size_t)phys - (j * 0x1000));
        kmemcpy(init_phys, (void *)(elf_phdr[i].p_paddr), elf_phdr[i].p_filesz);
    }

    /* Create new process for this program */
    pid_t new_pid = task_pcreate(&new_pagemap);
    if (new_pid == (size_t)-1) return -1;

    /* Allocate some stack for the entry point */
    size_t *stack = kalloc(2048 * sizeof(size_t));

    size_t entry_point = (size_t)elf_hdr->entry;
    void *(*entry)(void *) = (void *)entry_point;

    /* Spawn the entry thread in the new process */
    tid_t new_tid = task_tcreate(new_pid, &stack[2047], entry, (void *)0);
    if (new_tid == (size_t)-1) return -1;

    return 0;
}
