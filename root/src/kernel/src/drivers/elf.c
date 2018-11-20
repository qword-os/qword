#include <elf.h>
#include <klib.h>
#include <task.h>
#include <fs.h>
#include <mm.h>
#include <panic.h>

/* Execute an ELF file given some file data
   out_ld_path: If non-null, returns path of the dynamic linker as kalloc()ed string */
int elf_load(int fd, struct pagemap_t *pagemap, size_t base, struct auxval_t *auxval,
        char **out_ld_path) {
    char *ld_path = NULL;

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

    auxval->at_phdr = 0;
    auxval->at_phent = sizeof(struct elf_phdr_t);
    auxval->at_phnum = hdr.ph_num;

    for (size_t i = 0; i < hdr.ph_num; i++) {
        if (phdr[i].p_type == PT_INTERP) {
            if (!out_ld_path)
                continue;

            ld_path = kalloc(phdr[i].p_filesz + 1);
            if (!ld_path) {
                kfree(phdr);
                return -1;
            }

            ret = lseek(fd, phdr[i].p_offset, SEEK_SET);
            if (ret == -1) {
                kfree(phdr);
                kfree(ld_path);
                return -1;
            }

            ret = read(fd, ld_path, phdr[i].p_filesz);
            if (ret == -1) {
                kfree(phdr);
                kfree(ld_path);
                return -1;
            }
            ld_path[phdr[i].p_filesz] = 0;
        } else if (phdr[i].p_type == PT_PHDR) {
            auxval->at_phdr = base + phdr[i].p_vaddr;
        } else if (phdr[i].p_type != PT_LOAD)
            continue;

        size_t page_count = (phdr[i].p_memsz + (PAGE_SIZE - 1)) / PAGE_SIZE;

        /* Allocate space */
        void *addr = pmm_alloc(page_count);
        if (!addr) {
            kfree(phdr);
            kfree(ld_path);
            return -1;
        }

        size_t pf = 0x05;
        if(phdr[i].p_flags & PF_W)
            pf |= 0x02;
        for (size_t j = 0; j < page_count; j++) {
            size_t virt = base + phdr[i].p_vaddr + (j * PAGE_SIZE);
            size_t phys = (size_t)addr + (j * PAGE_SIZE);
            map_page(pagemap, phys, virt, pf);
        }

        char *buf = (char *)((size_t)addr + MEM_PHYS_OFFSET);
        kmemset(buf, 0, page_count * PAGE_SIZE);

        ret = lseek(fd, phdr[i].p_offset, SEEK_SET);
        if (ret == -1) {
            kfree(phdr);
            kfree(ld_path);
            return -1;
        }

        ret = read(fd, buf + (phdr[i].p_vaddr & (PAGE_SIZE - 1)), phdr[i].p_filesz);
        if (ret == -1) {
            kfree(phdr);
            kfree(ld_path);
            return -1;
        }
    }

    kfree(phdr);

    auxval->at_entry = base + hdr.entry;
    if (out_ld_path)
        *out_ld_path = ld_path;
    return 0;
}
