#ifndef __STIVALE__H
#define __STIVALE__H

#include <stdint.h>

enum stivale_memmap_entry_type_t {
    USABLE      = 1,
    RESERVED    = 2,
    ACPIRECLAIM = 3,
    ACPINVS     = 4
};

struct stivale_memmap_entry_t {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    uint32_t unused;
} __attribute__((packed));

struct stivale_memmap_t {
    struct stivale_memmap_entry_t *address;
    uint64_t entries;
} __attribute__((packed));

struct stivale_framebuffer_t {
    uint64_t address;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
} __attribute__((packed));

struct stivale_module_t {
    uint64_t  begin;
    uint64_t  end;
    char      name[128];
    uint64_t  next;
} __attribute__((packed));

struct stivale_struct_t {
    char* cmdline;
    struct stivale_memmap_t memmap;
    struct stivale_framebuffer_t fb;
    uint64_t rsdp;
    uint64_t module_count;
    struct stivale_module_t module[];
} __attribute__((packed));

#endif
