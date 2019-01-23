#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>
#include <user/task.h>

#define KPRN_MAX_TYPE 3

#define KPRN_INFO   0
#define KPRN_WARN   1
#define KPRN_ERR    2
#define KPRN_DBG    3
#define KPRN_PANIC  4

#define EMPTY ((void *)(size_t)(-1))

__attribute__((always_inline)) inline void atomic_fetch_add_int(int *p, int *v, int x) {
    int h = x;
    asm volatile (
        "lock xadd dword ptr [%1], %0;"
        : "+r" (h)
        : "r" (p)
        : "memory"
    );
    *v = h;
}

__attribute__((always_inline)) inline void atomic_add_uint64_relaxed(uint64_t *p, uint64_t x) {
    asm volatile (
        "lock xadd qword ptr [%1], %0;"
        : "+r" (x)
        : "r" (p)
    );
}

#define dynarray_new(type, name) \
    static type **name; \
    static size_t name##_i = 0; \
    static lock_t name##_lock = 1;

#define public_dynarray_new(type, name) \
    type **name; \
    size_t name##_i = 0; \
    lock_t name##_lock = 1;

#define public_dynarray_prototype(type, name) \
    extern type **name; \
    extern size_t name##_i; \
    extern lock_t name##_lock;

#define dynarray_add(type, name, element) ({ \
    __label__ fnd; \
    __label__ out; \
    int ret = -1; \
        \
    spinlock_acquire(&name##_lock); \
        \
    size_t i; \
    for (i = 0; i < name##_i; i++) { \
        if (!name[i]) \
            goto fnd; \
    } \
        \
    name##_i += 256; \
    void *tmp = krealloc(name, name##_i * sizeof(type *)); \
    if (!tmp) \
        goto out; \
    name = tmp; \
        \
fnd: \
    name[i] = kalloc(sizeof(type)); \
    if (!name[i]) \
        goto out; \
    *name[i] = *element; \
        \
    ret = i; \
        \
out: \
    spinlock_release(&name##_lock); \
    ret; \
})

#define dynarray_search(type, name, cond) ({ \
    __label__ fnd; \
    __label__ out; \
    int ret = -1; \
        \
    spinlock_acquire(&name##_lock); \
        \
    size_t i; \
    for (i = 0; i < name##_i; i++) { \
        if (!name[i]) \
            continue; \
        if (name[i]->cond) \
            goto fnd; \
    } \
    goto out; \
        \
fnd: \
    ret = i; \
        \
out: \
    spinlock_release(&name##_lock); \
    ret; \
})

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

int exec(pid_t, const char *, const char **, const char **);

pid_t kexec(const char *, const char **, const char **,
            const char *, const char *, const char *);

void execve_send_request(pid_t, const char *, const char **, const char **, lock_t **, int **);
void exit_send_request(pid_t, int);
void userspace_request_monitor(void *);

int ktolower(int);
char *kstrchrnul(const char *, int);
char *kstrcpy(char *, const char *);
size_t kstrlen(const char *);
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, size_t);
void kprint(int type, const char *fmt, ...);
void *kalloc(size_t);
void kfree(void *);
void *krealloc(void *, size_t);

void *kmemset(void *, int, size_t);
void *kmemset64(void *, uint64_t, size_t);
void *kmemcpy(void *, const void *, size_t);
void *kmemcpy64(void *, const void *, size_t);
int kmemcmp(const void *, const void *, size_t);
void *kmemmove(void *, const void *, size_t);

void readline(int, const char *, char *, size_t);

int ht_init(struct hashtable_t *, int);
int ht_add(struct hashtable_t *, struct ht_entry_t*, uint64_t);
struct ht_entry_t *ht_get_bucket(struct hashtable_t *, uint64_t);
struct ht_entry_t *ht_remove_entry(struct hashtable_t*,
        struct ht_entry_t*, struct ht_entry_t*);
uint64_t ht_hash_str(const char *);

typedef int64_t off_t;

#endif
