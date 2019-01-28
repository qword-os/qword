#include <stddef.h>
#include <lib/ht.h>
#include <lib/alloc.h>

int ht_init(struct hashtable_t *table, int size) {
    table->size = size;
    table->num_entries = 0;
    table->buckets = kalloc(sizeof(struct ht_entry_t*) * table->size);
    if (!table->buckets)
        return -1;
    for (int i = 0; i < table->size; i++)
        table->buckets[i] = NULL;
    return 0;
}

int ht_add(struct hashtable_t *table, struct ht_entry_t *new_entry,
        uint64_t hash) {
    /* if load factor is greater than or equal to 0.75, reallocate */
    if (4*(table->num_entries + 1) >= 3*(table->size)) {
        struct hashtable_t temp_table;
        ht_init(&temp_table, table->size * 2);

        for (int i = 0; i < table->size; i++) {
            struct ht_entry_t *bucket = table->buckets[i];
            for (; bucket; bucket = bucket->next)
                ht_add(&temp_table, bucket, bucket->hash);
        }

        table->size = temp_table.size;
        table->num_entries = temp_table.num_entries;
        kfree(table->buckets);
        table->buckets = temp_table.buckets;
    }
    int pos = (hash & (table->size - 1));

    if (table->buckets[pos]) {
        struct ht_entry_t *entry = NULL;
        for (entry = table->buckets[pos]; entry->next; entry = entry->next);
        entry->next = new_entry;
        entry->next->next = NULL;
        entry->next->hash = hash;
        table->num_entries++;
        return 0;
    }

    table->buckets[pos] = new_entry;
    table->buckets[pos]->next= NULL;
    table->buckets[pos]->hash = hash;
    table->num_entries++;
    return 0;
}

struct ht_entry_t *ht_get_bucket(struct hashtable_t *table, uint64_t hash) {
    int pos = (hash & (table->size - 1));
    if (pos >= table->size)
        return NULL;
    return table->buckets[pos];
}

struct ht_entry_t *ht_remove_entry(struct hashtable_t *table,
        struct ht_entry_t *entry, struct ht_entry_t *prev) {
    if (!prev) {
        int pos = (entry->hash & (table->size - 1));
        table->buckets[pos] = entry->next;
        table->num_entries--;
        return entry;
    }

    prev->next = entry->next;
    table->num_entries--;
    return entry;
}

uint64_t ht_hash_str(const char *str) {
    /* djb2
     * http://www.cse.yorku.ca/~oz/hash.html
     */
    uint64_t hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c;
    return hash;
}
