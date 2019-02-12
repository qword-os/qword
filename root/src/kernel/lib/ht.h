#ifndef __HT_H__
#define __HT_H__

#include <stddef.h>
#include <lib/alloc.h>
#include <lib/lock.h>

#define ENTRIES_PER_HASHING_LEVEL 65536
#define MAX_HASHING_LEVELS 16

static uint64_t hashes_per_level[MAX_HASHING_LEVELS] = {
    5381,
    8907,
    231,
    712,
    9636,
    4903,
    667,
    5176,
    4266,
    2287,
    1960,
    5724,
    3568,
    7969,
    2530,
    3526
};

static inline uint64_t ht_hash_str(const char *str, int level) {
    /* djb2
     * http://www.cse.yorku.ca/~oz/hash.html
     */
    uint64_t hash = hashes_per_level[level];
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return (hash % ENTRIES_PER_HASHING_LEVEL);
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

static void **__ht_dump(void **ht, void **buf, size_t *size) {
    for (size_t i = 0; i < ENTRIES_PER_HASHING_LEVEL; i++) {
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
    hashtable = kalloc(ENTRIES_PER_HASHING_LEVEL * sizeof(void *)); \
    if (!hashtable) { \
        ret = -1; \
        goto out; \
    } \
    hashtable##_lock = new_lock; \
out: \
    ret; \
})

#define ht_get(type, hashtable, name) ({ \
    __label__ out; \
    type *ret; \
        \
    spinlock_acquire(&hashtable##_lock); \
    int current_level; \
    type **ht = hashtable; \
    for (current_level = 0; ; current_level++) { \
        uint64_t hash = ht_hash_str(name, current_level); \
        if (!ht[hash]) { \
            ret = NULL; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            ret = ht[hash]; \
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
    int current_level; \
    type **ht = hashtable; \
    for (current_level = 0; ; current_level++) { \
        uint64_t hash = ht_hash_str(element->name, current_level); \
        if (!ht[hash]) { \
            ht[hash] = element; \
            goto out; \
        } else if ((size_t)ht[hash] & 1) { \
            ht = (void *)((size_t)ht[hash] - 1); \
            continue; \
        } else { \
            if ( \
                   (!kstrcmp(element->name, ht[hash]->name)) \
                || (current_level == MAX_HASHING_LEVELS - 1) \
            ) { \
                ret = -1; \
                goto out; \
            } \
            type **new_ht = kalloc(ENTRIES_PER_HASHING_LEVEL * sizeof(void *)); \
            if (!new_ht) { \
                ret = -1; \
                goto out; \
            } \
            type *old_elem = ht[hash]; \
            ht[hash] = (void *)((size_t)new_ht + 1); \
            ht = new_ht; \
            uint64_t old_elem_hash = ht_hash_str(old_elem->name, current_level + 1); \
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
