#include <stddef.h>
#include <lib/alloc.h>
#include <lib/klib.h>
#include <mm/mm.h>

typedef struct {
    size_t pages;
    size_t size;
} alloc_metadata_t;

void *kalloc(size_t size) {
    size_t page_count = size / PAGE_SIZE;

    if (size % PAGE_SIZE) page_count++;

    char *ptr = pmm_allocz(page_count + 1);

    if (!ptr) {
        return (void *)0;
    }

    ptr += MEM_PHYS_OFFSET;

    alloc_metadata_t *metadata = (alloc_metadata_t *)ptr;
    ptr += PAGE_SIZE;

    metadata->pages = page_count;
    metadata->size = size;

    return (void *)ptr;
}

void kfree(void *ptr) {
    alloc_metadata_t *metadata = (alloc_metadata_t *)((size_t)ptr - PAGE_SIZE);

    pmm_free((void *)((size_t)metadata - MEM_PHYS_OFFSET), metadata->pages + 1);
}

void *krealloc(void *ptr, size_t new) {
    /* check if 0 */
    if (!ptr) return kalloc(new);
    if (!new) {
        kfree(ptr);
        return (void *)0;
    }

    /* Reference metadata page */
    alloc_metadata_t *metadata = (alloc_metadata_t *)((size_t)ptr - PAGE_SIZE);

    if ((metadata->size + PAGE_SIZE - 1) / PAGE_SIZE
         == (new + PAGE_SIZE - 1) / PAGE_SIZE) {
        metadata->size = new;
        return ptr;
    }

    char *new_ptr;
    if ((new_ptr = kalloc(new)) == 0) {
        return (void *)0;
    }

    if (metadata->size > new)
        /* Copy all the data from the old pointer to the new pointer,
         * within the range specified by `size`. */
        memcpy(new_ptr, (char *)ptr, new);
    else
        memcpy(new_ptr, (char *)ptr, metadata->size);

    kfree(ptr);

    return new_ptr;
}
