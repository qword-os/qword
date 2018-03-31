#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <e820.h>

#define MBITMAP_FULL ((0x100000000 / PAGE_SIZE) / 32)
size_t bitmap_full = MBITMAP_FULL;
#define BASE = 0x1000000 / PAGE_SIZE

static volatile uint32_t *mem_bitmap;
static volatile uint32_t initial_bitmap[MBITMAP_FULL];
static volatile uint32_t *tmp_bitmap;

static int read_bitmap(size_t i) {
    size_t which_entry = i / 32;
    size_t offset = i % 32;

    return (int)((mem_bitmap[which_entry] >> offset) & 1);
}

static int write_bitmap(size_t i, int val) {
    size_t which_entry = i / 32;
    size_t offset = i % 32;

    if (val)
        mem_bitmap[which_entry] |= (1 << offset);
    else
        mem_bitmap[which_entry] |= ~(1 << offset);
}

// Populate bitmap using e820 data.
void init_bitmap(void) {
    // TODO.
}

// Allocate physical memory.
void *pmm_alloc(size_t pg_count) {
    // TODO.
    return (void *)0;
}

void pmm_free(void *ptr, size_t pg_count) {
}


void bm_realloc(void) {}
