#include <elf.h>
#include <klib.h>
#include <task.h>
#include <fs.h>
#include <mm.h>
#include <panic.h>

#define PROCESS_IMAGE_PHDR_LOCATION     ((size_t)0x0000600000000000)

/* Execute an ELF file given some file data */
int elf_load(int fd, struct pagemap_t *pagemap, struct auxval_t *auxval) {
    int ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) return -1;

    char *magic = "\177ELF";

    struct elf_hdr_t hdr;

    ret = read(fd, &hdr, sizeof(struct elf_hdr_t));
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

    struct elf_phdr_t *phdr = kalloc(hdr.ph_num * sizeof(struct elf_phdr_t));
    if (!phdr) return -1;

    ret = read(fd, phdr, hdr.ph_num * sizeof(struct elf_phdr_t));
    if (ret == -1) {
        kfree(phdr);
        return -1;
    }

    /* Read phdr into address space */
    size_t phdr_size_in_pages = (hdr.ph_num * sizeof(struct elf_phdr_t)) / PAGE_SIZE;
    if ((hdr.ph_num * sizeof(struct elf_phdr_t)) % PAGE_SIZE) phdr_size_in_pages++;
    void *phdr_phys_addr = pmm_alloc(phdr_size_in_pages);
    void *phdr_virt_addr = phdr_phys_addr + MEM_PHYS_OFFSET;
    kmemcpy(phdr_virt_addr, phdr, hdr.ph_num * sizeof(struct elf_phdr_t));
    for (size_t i = 0; i < phdr_size_in_pages; i++) {
        map_page(pagemap, (size_t)phdr_phys_addr + i * PAGE_SIZE,
                 PROCESS_IMAGE_PHDR_LOCATION + i * PAGE_SIZE, 0x07);
    }

    auxval->at_phdr = PROCESS_IMAGE_PHDR_LOCATION;
    auxval->at_phent = sizeof(struct elf_phdr_t);
    auxval->at_phnum = hdr.ph_num;

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

    auxval->at_entry = hdr.entry;
    return 0;
}
