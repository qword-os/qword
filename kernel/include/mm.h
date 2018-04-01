#ifndef __MM_H__
#define __MM_H__

#include <stddef.h>

#define PAGE_SIZE 4096

void *pmm_alloc(size_t);
void pmm_free(void *, size_t);
void init_pmm(void);

#endif
