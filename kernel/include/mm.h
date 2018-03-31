#ifndef __MM_H__
#define __MM_H__

static uint64_t memory_size;

static int read_bitmap(size_t);
static int write_bitmap(size_t, int);
void bm_realloc(void);
void init_bitmap(void);
void *pmm_alloc(size_t);
void pmm_free(void *, size_t);

#define PAGE_SIZE 4096
#endif
