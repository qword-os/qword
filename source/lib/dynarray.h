#ifndef __DYNARRAY_H__
#define __DYNARRAY_H__

#include <stddef.h>
#include <lib/lock.h>
#include <lib/alloc.h>

#define dynarray_new(type, name) \
    static struct { \
        int refcount; \
        int present; \
        type data; \
    } **name; \
    static size_t name##_i = 0; \
    static lock_t name##_lock = new_lock;

#define public_dynarray_new(type, name) \
    struct __##name##_struct **name; \
    size_t name##_i = 0; \
    lock_t name##_lock = new_lock;

#define public_dynarray_prototype(type, name) \
    struct __##name##_struct { \
        int refcount; \
        int present; \
        type data; \
    }; \
    extern struct __##name##_struct **name; \
    extern size_t name##_i; \
    extern lock_t name##_lock;

#define dynarray_remove(dynarray, element) ({ \
    __label__ out; \
    int ret; \
    spinlock_acquire(&dynarray##_lock); \
    if (!dynarray[element]) { \
        ret = -1; \
        goto out; \
    } \
    ret = 0; \
    dynarray[element]->present = 0; \
    if (!locked_dec(&dynarray[element]->refcount)) { \
        kfree(dynarray[element]); \
        dynarray[element] = 0; \
    } \
out: \
    spinlock_release(&dynarray##_lock); \
    ret; \
})

#define dynarray_unref(dynarray, element) ({ \
    spinlock_acquire(&dynarray##_lock); \
    if (dynarray[element] && !locked_dec(&dynarray[element]->refcount)) { \
        kfree(dynarray[element]); \
        dynarray[element] = 0; \
    } \
    spinlock_release(&dynarray##_lock); \
})

#define dynarray_getelem(type, dynarray, element) ({ \
    spinlock_acquire(&dynarray##_lock); \
    type *ptr = NULL; \
    if (dynarray[element] && dynarray[element]->present) { \
        ptr = &dynarray[element]->data; \
        locked_inc(&dynarray[element]->refcount); \
    } \
    spinlock_release(&dynarray##_lock); \
    ptr; \
})

#define dynarray_add(type, dynarray, element) ({ \
    __label__ fnd; \
    __label__ out; \
    int ret = -1; \
        \
    spinlock_acquire(&dynarray##_lock); \
        \
    size_t i; \
    for (i = 0; i < dynarray##_i; i++) { \
        if (!dynarray[i]) \
            goto fnd; \
    } \
        \
    dynarray##_i += 256; \
    void *tmp = krealloc(dynarray, dynarray##_i * sizeof(void *)); \
    if (!tmp) \
        goto out; \
    dynarray = tmp; \
        \
fnd: \
    dynarray[i] = kalloc(sizeof(**dynarray)); \
    if (!dynarray[i]) \
        goto out; \
    dynarray[i]->refcount = 1; \
    dynarray[i]->present = 1; \
    dynarray[i]->data = *element; \
        \
    ret = i; \
        \
out: \
    spinlock_release(&dynarray##_lock); \
    ret; \
})

#define dynarray_search(type, dynarray, cond) ({ \
    __label__ fnd; \
    __label__ out; \
    type *ret = NULL; \
        \
    spinlock_acquire(&dynarray##_lock); \
        \
    size_t i; \
    for (i = 0; i < dynarray##_i; i++) { \
        if (!dynarray[i] || !dynarray[i]->present) \
            continue; \
        type *elem = &dynarray[i]->data; \
        if (cond) \
            goto fnd; \
    } \
    goto out; \
        \
fnd: \
    ret = &dynarray[i]->data; \
    locked_inc(&dynarray[i]->refcount); \
        \
out: \
    spinlock_release(&dynarray##_lock); \
    ret; \
})

#endif
