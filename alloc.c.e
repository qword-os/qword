#include <stddef.h>
#include <lib/alloc.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <sys/panic.h>
#include <mm/mm.h>

#define POOL_SIZE_IN_PAGES 2050

static lock_t alloc_lock = new_lock;

struct pool_chunk_t {
    size_t free;
    size_t size;
    size_t prev_chunk;
    size_t padding;
};

struct alloc_pool_t {
    struct pool_chunk_t *pool;
    struct alloc_pool_t *next;
};

static struct alloc_pool_t root_pool = {0};

void init_alloc(void) {
    root_pool.pool = (pmm_alloc(POOL_SIZE_IN_PAGES) + MEM_PHYS_OFFSET);

    // creates the first pool chunk

    root_pool.pool->free = 1;
    root_pool.pool->size = (POOL_SIZE_IN_PAGES * PAGE_SIZE) - sizeof(struct pool_chunk_t);
    root_pool.pool->prev_chunk = 0;
}

void *kalloc(size_t size) {
    panic_if(size > (POOL_SIZE_IN_PAGES * PAGE_SIZE - sizeof(struct pool_chunk_t)));

    spinlock_acquire(&alloc_lock);

    // search for a big enough, free heap chunk
    struct alloc_pool_t *pool = &root_pool;
    struct pool_chunk_t *pool_chunk = root_pool.pool;
    size_t pool_base = (size_t)root_pool.pool;
    size_t pool_chunk_ptr;
    void *area;

    // align all allocations to 16
    size = size & 0xf ? (size + 0x10) & ~((size_t)(0xf)) : size;

    for (;;) {
        if ((pool_chunk->free) && (pool_chunk->size > (size + sizeof(struct pool_chunk_t)))) {
            // split off a new pool_chunk
            struct pool_chunk_t *new_chunk;
            new_chunk = (struct pool_chunk_t *)((size_t)pool_chunk + size + sizeof(struct pool_chunk_t));
            new_chunk->free = 1;
            new_chunk->size = pool_chunk->size - (size + sizeof(struct pool_chunk_t));
            new_chunk->prev_chunk = (size_t)pool_chunk;
            // resize the old chunk
            pool_chunk->free = 0;
            pool_chunk->size = size;
            // tell the next chunk where the old chunk is now
            struct pool_chunk_t *next_chunk;
            next_chunk = (struct pool_chunk_t *)((size_t)new_chunk + new_chunk->size + sizeof(struct pool_chunk_t));
            next_chunk->prev_chunk = (size_t)new_chunk;
            area = (void *)((size_t)pool_chunk + sizeof(struct pool_chunk_t));
            break;
        } else {
            pool_chunk_ptr = (size_t)pool_chunk;
            pool_chunk_ptr += pool_chunk->size + sizeof(struct pool_chunk_t);
            if (pool_chunk_ptr >= pool_base + (POOL_SIZE_IN_PAGES * PAGE_SIZE)) {
                // pool exhausted
                if (pool->next) {
                    // next pool already present
                    pool = pool->next;
                    pool_chunk = pool->pool;
                    pool_base = (size_t)pool_chunk;
                    continue;
                } else {
                    // allocate next pool
                    pool->next = (pmm_allocz(1) + MEM_PHYS_OFFSET);
                    if (pool->next == (void *)MEM_PHYS_OFFSET) {
                        pool->next = NULL;
                        spinlock_release(&alloc_lock);
                        return NULL;
                    }
                    pool->next->pool = (pmm_alloc(POOL_SIZE_IN_PAGES) + MEM_PHYS_OFFSET);
                    if (pool->next->pool == (void *)MEM_PHYS_OFFSET) {
                        pmm_free((void *)pool->next - MEM_PHYS_OFFSET, 1);
                        pool->next = NULL;
                        spinlock_release(&alloc_lock);
                        return NULL;
                    }
                    pool = pool->next;
                    pool_chunk = pool->pool;
                    pool_chunk->free = 1;
                    pool_chunk->size = (POOL_SIZE_IN_PAGES * PAGE_SIZE) - sizeof(struct pool_chunk_t);
                    pool_chunk->prev_chunk = 0;
                    pool_base = (size_t)pool_chunk;
                    continue;
                }
            }
            pool_chunk = (struct pool_chunk_t *)pool_chunk_ptr;
            continue;
        }
    }

    // zero the memory
    kmemset(area, 0, size);

    spinlock_release(&alloc_lock);
    return area;
}

void *krealloc(void *addr, size_t new_size) {
    if (!addr)
        return kalloc(new_size);

    if (!new_size) {
        kfree(addr);
        return NULL;
    }

    spinlock_acquire(&alloc_lock);
    size_t pool_chunk_ptr = (size_t)addr;

    pool_chunk_ptr -= sizeof(struct pool_chunk_t);
    struct pool_chunk_t *pool_chunk = (struct pool_chunk_t *)pool_chunk_ptr;

    size_t old_size = pool_chunk->size;
    spinlock_release(&alloc_lock);

    void *new_ptr;
    if (!(new_ptr = kalloc(new_size)))
        return NULL;

    new_size = new_size & 0xf ? (new_size + 0x10) & ~((size_t)(0xf)) : new_size;

    if (old_size > new_size)
        kmemcpy(new_ptr, addr, new_size);
    else
        kmemcpy(new_ptr, addr, old_size);

    kfree(addr);

    return new_ptr;
}

void kfree(void *addr) {
    spinlock_acquire(&alloc_lock);

    size_t pool_chunk_ptr = (size_t)addr;

    pool_chunk_ptr -= sizeof(struct pool_chunk_t);
    struct pool_chunk_t *pool_chunk = (struct pool_chunk_t *)pool_chunk_ptr;

    pool_chunk_ptr += pool_chunk->size + sizeof(struct pool_chunk_t);
    struct pool_chunk_t *next_chunk = (struct pool_chunk_t *)pool_chunk_ptr;

    struct pool_chunk_t *prev_chunk = (struct pool_chunk_t *)pool_chunk->prev_chunk;

    // flag chunk as free
    pool_chunk->free = 1;

    if ((size_t)next_chunk >= POOL_SIZE_IN_PAGES * PAGE_SIZE) goto skip_next_chunk;

    // if the next chunk is free as well, fuse the chunks into a single one
    if (next_chunk->free) {
        pool_chunk->size += next_chunk->size + sizeof(struct pool_chunk_t);
        // update next chunk ptr
        next_chunk = (struct pool_chunk_t *)((size_t)next_chunk + next_chunk->size + sizeof(struct pool_chunk_t));
        // update new next chunk's prev to ourselves
        next_chunk->prev_chunk = (size_t)pool_chunk;
    }

skip_next_chunk:
    // if the previous chunk is free as well, fuse the chunks into a single one
    if (prev_chunk) {       // if its not the first chunk
        if (prev_chunk->free) {
            prev_chunk->size += pool_chunk->size + sizeof(struct pool_chunk_t);
            // notify the next chunk of the change
            if ((size_t)next_chunk < POOL_SIZE_IN_PAGES * PAGE_SIZE)
                next_chunk->prev_chunk = (size_t)prev_chunk;
        }
    }

    spinlock_release(&alloc_lock);
    return;
}
