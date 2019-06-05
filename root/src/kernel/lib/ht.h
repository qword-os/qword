#ifndef __HT_H__
#define __HT_H__

#include <stddef.h>
#include <lib/alloc.h>
#include <mm/mm.h>
#include <lib/lock.h>
#include <lib/rand.h>

#define ENTRIES_PER_HASHING_LEVEL_BITS 14
#define ENTRIES_PER_HASHING_LEVEL \
        ((size_t)1 << ENTRIES_PER_HASHING_LEVEL_BITS)

// tries to shrink down a hash to 'bits' bits without losing entropy
__attribute__((unused)) __attribute__((always_inline))
static inline uint64_t minify_hash(uint64_t hash, int bits) {
    uint64_t bmask = ((uint64_t)1 << bits) - 1;
    uint64_t h = hash & bmask;
    int b = 1;
    while (b + bits < 64) {
        h ^= (hash & (bmask << b)) >> b;
        b++;
    }
    return h;
}

__attribute__((unused)) __attribute__((always_inline))
static inline uint64_t ht_hash_str(const char *str, uint64_t seed) {
    /* djb2
     * http://www.cse.yorku.ca/~oz/hash.html
     */
    int c;
    while ((c = *str++))
        seed = ((seed << 5) + seed) + c;
    uint64_t mh = minify_hash(seed, ENTRIES_PER_HASHING_LEVEL_BITS);
    if (!mh)
        return 1;
    else
        return mh;
}

__attribute__((unused)) __attribute__((always_inline))
static inline uint64_t ht_hash_u64(uint64_t c, uint64_t seed) {
    // based on one of the replies on
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
    c = (c ^ (c >> 30)) * seed;
    c = (c ^ (c >> 27)) * seed;
    c = c ^ (c >> 31);
    uint64_t mh = minify_hash(c, ENTRIES_PER_HASHING_LEVEL_BITS);
    if (!mh)
        return 1;
    else
        return mh;
}

#define ht_new(type, name) \
    type **name; \
    lock_t name##_lock;

#define ht_dump(type, hashtable, size) ({ \
    void **buf = NULL; \
    *size = 0; \
    type **ret = (type **)__ht_dump((void **)hashtable, buf, size); \
    ret; \
})

__attribute__((unused)) static void **__ht_dump(void **ht, void **buf, size_t *size) {
    for (size_t i = 1; i < ENTRIES_PER_HASHING_LEVEL; i++) {
        if (!ht[i]) {
            continue;
        } else if ((size_t)ht[i] & 1) {
            void **tmp = __ht_dump((void *)((size_t)ht[i] - 1), buf, size);
            if (!tmp)
                return NULL;
            buf = tmp;
        } else {
            void **tmp = krealloc(buf, sizeof(void *) * (*size + 1));
            if (!tmp) {
                kfree(buf);
                return NULL;
            }
            buf = tmp;
            buf[(*size)++] = ht[i];
        }
    }

    return buf;
}

#define ht_init(hashtable) ({ \
    __label__ out; \
    int ret = 0; \
    hashtable = pmm_allocz((ENTRIES_PER_HASHING_LEVEL * sizeof(void *)) / PAGE_SIZE); \
    if (!hashtable) { \
        ret = -1; \
        goto out; \
    } \
    hashtable = (void *)hashtable + MEM_PHYS_OFFSET; \
    hashtable##_lock = new_lock; \
    while (!(hashtable[0] = (void *)rand64())); \
out: \
    ret; \
})

#define ht_get(type, hashtable, nname) ({ \
    __label__ out; \
    type *ret; \
        \
    spinlock_acquire(&hashtable##_lock); \
    type **ht = hashtable; \
    for (;;) { \
        uint64_t hash = ht_hash_str(nname, (uint64_t)ht[0]); \
        if (!ht[hash]) { \
            ret = NULL; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            if (strcmp(nname, ((type **)ht)[hash]->name)) { \
                ret = NULL; \
                goto out; \
            } \
            ret = ht[hash]; \
            goto out; \
        } \
    } \
out: \
    spinlock_release(&hashtable##_lock); \
    ret; \
})

#define ht_remove(type, hashtable, nname) ({ \
    __label__ out; \
    type *ret; \
        \
    spinlock_acquire(&hashtable##_lock); \
    type **ht = hashtable; \
    for (;;) { \
        uint64_t hash = ht_hash_str(nname, (uint64_t)ht[0]); \
        if (!ht[hash]) { \
            ret = NULL; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            if (strcmp(nname, ((type **)ht)[hash]->name)) { \
                ret = NULL; \
                goto out; \
            } \
            ret = ht[hash]; \
            ht[hash] = 0; \
            goto out; \
        } \
    } \
out: \
    spinlock_release(&hashtable##_lock); \
    ret; \
})

// Adds an element to a hash table with these prerequisites:
// the element shall be a pointer to a structure containing a "name"
// element which is of type "char *". This "name" element shall be used
// for hashing purposes.
#define ht_add(type, hashtable, element) ({ \
    __label__ out; \
    int ret = 0; \
        \
    spinlock_acquire(&hashtable##_lock); \
    type **ht = hashtable; \
        \
    for (;;) { \
        uint64_t hash = ht_hash_str(element->name, (uint64_t)ht[0]); \
        if (!ht[hash]) { \
            ht[hash] = element; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            if (!strcmp(element->name, ht[hash]->name)) { \
                ret = -1; \
                goto out; \
            } \
            type **new_ht = pmm_allocz((ENTRIES_PER_HASHING_LEVEL * sizeof(void *)) / PAGE_SIZE); \
            if (!new_ht) { \
                ret = -1; \
                goto out; \
            } \
            new_ht = (void *)new_ht + MEM_PHYS_OFFSET; \
            type *old_elem = ht[hash]; \
            ht[hash] = (void *)((size_t)new_ht + 1); \
            ht = new_ht; \
            while (!(ht[0] = (void *)rand64())); \
            uint64_t old_elem_hash = ht_hash_str(old_elem->name, (uint64_t)ht[0]); \
            ht[old_elem_hash] = old_elem; \
            continue; \
        } \
    } \
    \
out: \
    spinlock_release(&hashtable##_lock); \
    ret; \
})

// Adds an element to a hash table with these prerequisites:
// the element shall be a pointer to a structure containing a "value"
// element which is of an integer type. This "value" element shall be used
// for hashing purposes.
#define hti_add(type, hashtable, element) ({ \
    __label__ out; \
    int ret = 0; \
        \
    spinlock_acquire(&hashtable##_lock); \
    type **ht = hashtable; \
        \
    for (;;) { \
        uint64_t hash = ht_hash_u64((uint64_t)(element->value), (uint64_t)ht[0]); \
        if (!ht[hash]) { \
            ht[hash] = element; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            if (element->value == ht[hash]->value) { \
                ret = -1; \
                goto out; \
            } \
            type **new_ht = pmm_allocz((ENTRIES_PER_HASHING_LEVEL * sizeof(void *)) / PAGE_SIZE); \
            if (!new_ht) { \
                ret = -1; \
                goto out; \
            } \
            new_ht = (void *)new_ht + MEM_PHYS_OFFSET; \
            type *old_elem = ht[hash]; \
            ht[hash] = (void *)((size_t)new_ht + 1); \
            ht = new_ht; \
            while (!(ht[0] = (void *)rand64())); \
            uint64_t old_elem_hash = ht_hash_u64((uint64_t)(old_elem->value), (uint64_t)ht[0]); \
            ht[old_elem_hash] = old_elem; \
            continue; \
        } \
    } \
    \
out: \
    spinlock_release(&hashtable##_lock); \
    ret; \
})

#endif
