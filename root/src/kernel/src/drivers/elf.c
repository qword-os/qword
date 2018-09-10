#include <elf.h>
#include <klib.h>
#include <task.h>
#include <fs.h>
#include <mm.h>
#include <panic.h>

/* Execute an ELF file given some file data */
int elf_load(int fd, struct pagemap *pagemap, uint64_t *entry) {
    int ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) return -1;

    char *magic = "\177ELF";

    struct elf_hdr hdr;

    ret = read(fd, &hdr, sizeof(struct elf_hdr));
    if (ret == -1) return -1;

    for (size_t i = 0; i < 4; i++) {
        if (hdr.ident[i] != magic[i]) return -1;
    }

    if (hdr.ident[EI_CLASS] != 0x02) return -1;
    if (hdr.ident[EI_DATA] != BITS_LE) return -1;
    if (hdr.ident[EI_OSABI] != ABI_SYSV) return -1;
    if (hdr.machine != ARCH_X86_64) return -1;

    uint64_t phoff = hdr.phoff;
    ret = lseek(fd, phoff, SEEK_SET);
    if (ret == -1) return -1;

    struct elf_phdr *phdr = kalloc(hdr.ph_num * sizeof(struct elf_phdr));
    if (!phdr) return -1;

    ret = read(fd, phdr, hdr.ph_num * sizeof(struct elf_phdr));
    if (ret == -1) {
        kfree(phdr);
        return -1;
    }

    for (size_t i = 0; i < hdr.ph_num; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        size_t page_count = phdr[i].p_memsz / 0x1000;
        if (phdr[i].p_memsz % 0x1000) page_count++;

        /* Allocate space */
        void *addr = pmm_alloc(page_count);
        if (!addr) {
            kfree(phdr);
            return -1;
        }

        for (size_t j = 0; j < page_count; j++) {
            size_t virt = phdr[i].p_vaddr + (j * 0x1000);
            size_t phys = (size_t)addr + (j * 0x1000);
            map_page(pagemap, phys, virt, 0x07);
        }

        void *buf = (void *)((size_t)addr + MEM_PHYS_OFFSET);

        ret = lseek(fd, phdr[i].p_offset, SEEK_SET);
        if (ret == -1) {
            kfree(phdr);
            return -1;
        }

        ret = read(fd, buf, phdr[i].p_filesz);
        if (ret == -1) {
            kfree(phdr);
            return -1;
        }
    }

    kfree(phdr);

    *entry = hdr.entry;
    return 0;
}
