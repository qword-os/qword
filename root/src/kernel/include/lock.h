#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>

typedef volatile uint64_t lock_t;

#define spinlock_acquire(lock) ({ \
    asm volatile ( \
        "xor rax, rax;" \
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
        "xor rax, rax;" \
        "lock xchg rax, qword ptr ds:[rbx];" \
        : "=a" (ret) \
        : "b" (lock) \
        : \
    ); \
    ret; \
})

#define spinlock_release(lock) ({ \
    asm volatile ( \
        "xor rax, rax;" \
        "inc rax;" \
        "lock xchg rax, qword ptr ds:[rbx];" \
        : \
        : "b" (lock) \
        : "rax" \
    ); \
})

#endif
