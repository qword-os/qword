#include <elf.h>
#include <klib.h>
#include <task.h>
#include <fs.h>
#include <mm.h>
#include <panic.h>

/* Execute an ELF file given some file data */
int elf_load(int fd, pagemap_t *pagemap, uint64_t *entry) {
    int ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) return -1;
    
    char *magic = "\177ELF";
    
    elf_hdr_t hdr;

    ret = read(fd, &hdr, sizeof(elf_hdr_t));
    if (ret == -1) return -1;

    for (size_t i = 0; i < 4; i++) {
        kprint(KPRN_DBG, "%c", hdr.ident[i]);
        if (hdr.ident[i] != magic[i]) return -1;
    }

    for (size_t i = 4; i < 15; i++) {
        kprint(KPRN_DBG, "%u", hdr.ident[i]);
    }

    if (hdr.ident[EI_CLASS] != 0x02) return -1;
    if (hdr.ident[EI_DATA] != BITS_LE) return -1;
    if (hdr.ident[EI_OSABI] != ABI_SYSV) return -1;
    if (hdr.machine != ARCH_X86_64) return -1;

    uint64_t phoff = hdr.phoff;
    ret = lseek(fd, phoff, SEEK_SET);
    if (ret == -1) {
        kprint(KPRN_DBG, "Failed to seek to program header table offset.");
        return -1;
    }

    elf_phdr_t *phdr = kalloc(hdr.ph_num * sizeof(elf_phdr_t));
    ret = read(fd, phdr, hdr.ph_num * sizeof(elf_phdr_t));
    if (ret == -1) {
        kprint(KPRN_DBG, "Failed to read program header table into memory");
        return -1;
    }


    for (size_t i = 0; i < hdr.ph_num; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue; 

        size_t page_count = phdr[i].p_memsz / 0x1000;
        if (page_count % 0x1000) page_count++;
        
        /* Allocate space */
        void *addr = pmm_alloc(page_count);

        for (size_t j = 0; j < page_count; j++) {
            size_t virt = phdr[i].p_vaddr + (j * 0x1000);
            size_t phys = (size_t)addr + (j * 0x1000);
            map_page(pagemap, phys, virt, 0x07);
        }

        void *buf = (size_t)addr + MEM_PHYS_OFFSET;
        
        ret = lseek(fd, phdr[i].p_offset, SEEK_SET);
        if (ret == -1) return -1;

        ret = read(fd, buf, phdr[i].p_filesz);
        if (ret == -1) return -1;
    }
    
    kprint(KPRN_DBG, "ELF load successful!");

    *entry = hdr.entry;
    return 0;
}

