#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>
#include <stddef.h>
#include <task.h>
#include <mm.h>

#define PT_LOAD     0x00000001
#define PT_INTERP   0x00000003

#define ABI_SYSV 0x00
#define ARCH_X86_64 0x3e
#define BITS_LE 0x01

/* Indices into identification array */
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define	EI_OSABI	7

struct elf_hdr_t {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t phdr_size;
    uint16_t ph_num;
    uint16_t shdr_size;
    uint16_t sh_num;
    uint16_t shstrndx;
};

struct elf_phdr_t {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf_shdr_t {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addr_align;
    uint64_t sh_entsize;
};

int elf_load(int, struct pagemap_t *, size_t, struct auxval_t *, char **);

#endif
