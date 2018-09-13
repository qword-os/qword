#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>

typedef volatile int64_t lock_t;

#define spinlock_inc(lock) ({ \
    asm volatile ( \
        "lock inc qword ptr ds:[rbx];" \
        : \
        : "b" (lock) \
    ); \
})

#define spinlock_read(lock) ({ \
    lock_t ret; \
    asm volatile ( \
        "xor eax, eax;" \
        "lock xadd qword ptr ds:[rbx], rax;" \
        : "=a" (ret) \
        : "b" (lock) \
    ); \
    ret; \
})

#define spinlock_acquire(lock) ({ \
    asm volatile ( \
        "xor eax, eax;" \
        "1: " \
        "lock xchg rax, qword ptr ds:[rbx];" \
        "test rax, rax;" \
        "jz 1b;" \
        : \
        : "b" (lock) \
        : "rax" \
    ); \
})

#define spinlock_test_and_acquire(lock) ({ \
    lock_t ret; \
    asm volatile ( \
        "xor eax, eax;" \
        "lock xchg rax, qword ptr ds:[rbx];" \
        : "=a" (ret) \
        : "b" (lock) \
        : \
    ); \
    ret; \
})

#define spinlock_release(lock) ({ \
    asm volatile ( \
        "xor eax, eax;" \
        "inc eax;" \
        "lock xchg rax, qword ptr ds:[rbx];" \
        : \
        : "b" (lock) \
        : "rax" \
    ); \
})

#endif
