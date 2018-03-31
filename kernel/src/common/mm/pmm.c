#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <klib.h>
#include <e820.h>

#define MBITMAP_FULL ((0x100000000 / PAGE_SIZE) / 32)
size_t bitmap_full = MBITMAP_FULL;
#define BASE 0x1000000 / PAGE_SIZE

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
    for (size_t i = 0; i < bitmap_full; i++) {
        initial_bitmap[i] = 0;
    }

    mem_bitmap = initial_bitmap;
    tmp_bitmap = kalloc(bitmap_full * sizeof(uint32_t));
    for (size_t i = 0; i < bitmap_full; i++)
        tmp_bitmap[i] = initial_bitmap[i];
    mem_bitmap = tmp_bitmap;
    
    // For each region specified by the e820, iterate over each page which 
    // fits in that region and if the region type indicates the area itself
    // is usable, write that page as free in the bitmap. Otherwise, mark the page as used.
    for (size_t i = 0; e820_map[i].type; i++) {
        for (size_t j; j * PAGE_SIZE < e820_map[i].length; i++) {
            size_t addr = e820_map[i].base + j * PAGE_SIZE;
            size_t page = addr / PAGE_SIZE;

            while (page >= bitmap_full * 32)
                /* TODO */
            if (e820_map[i].type == 1)
                write_bitmap(page, 0);
            else
                write_bitmap(page, 1);
        }
    }
}

// Allocate physical memory.
void *pmm_alloc(size_t pg_count) {
    // Allocate contiguous free pages.
    size_t counter = 0;
    size_t i;
    size_t start;

    for (i = BASE; i < bitmap_full * 32; i++) {
        if (!read_bitmap(i))
            counter++;
        else
            counter = 0;
        if (counter == pg_count)
            goto found;

    }

    return (void *)0;
found:
    start = i - (pg_count - 1);
    for (i = start; i < (start + pg_count); i++) {
        write_bitmap(i, 1);
    }
    
    // Return the physical address that represents the start of this physical page.
    return (void *)(start * PAGE_SIZE);
}

void pmm_free(void *ptr, size_t pg_count) {
    size_t start = (size_t)ptr / PAGE_SIZE;

    for (size_t i = start; i < (start + pg_count); i++) {
        write_bitmap(i, 0);
    }
}

void pmm_init(void) {
    init_bitmap();
}

void bm_realloc(void) {
    if ((tmp_bitmap = kalloc((bitmap_full + 2048) * sizeof(uint32_t))) == 0) {
        /* TODO panic here */
    }

    /* TODO */
}
