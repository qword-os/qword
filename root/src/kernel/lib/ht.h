#ifndef __HT_H__
#define __HT_H__

#include <stdint.h>

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

struct ht_entry_t {
    uint64_t hash;
    struct ht_entry_t *next;
};

struct hashtable_t {
    struct ht_entry_t **buckets;
    int num_entries;
    int size;
};

#define ht_get(table, hash, entry, predicate) for (entry = \
        ht_get_bucket(table, hash); entry; entry = entry->next) {  \
            if (predicate) break; }

#define ht_foreach(table, entry, body) for (int i = 0; i < (table)->size; i++) \
        {   entry = (table)->buckets[i]; \
            if (!entry) continue; \
            for (; entry->next; entry = entry->next){body}}

#define ht_remove(table, hash, entry, predicate) struct ht_entry_t *__prev = NULL; \
        for (entry = ht_get_bucket(table, hash); entry; entry = entry->next) { \
        if (predicate) {entry = ht_remove_entry(table, entry, __prev); break;} \
            __prev = entry}

int ht_init(struct hashtable_t *, int);
int ht_add(struct hashtable_t *, struct ht_entry_t*, uint64_t);
struct ht_entry_t *ht_get_bucket(struct hashtable_t *, uint64_t);
struct ht_entry_t *ht_remove_entry(struct hashtable_t*,
        struct ht_entry_t*, struct ht_entry_t*);
uint64_t ht_hash_str(const char *);

#endif
