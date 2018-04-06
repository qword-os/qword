#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

extern kernel_pagemap;

void *pmm_alloc(size_t);
void pmm_free(void *, size_t);
void init_pmm(void);

void map_page(uint64_t *, uint64_t, uint64_t, uint64_t);
int unmap_page(uint64_t *, uint64_t);
int remap_page(uint64_t *, uint64_t, uint64_t);

#endif
