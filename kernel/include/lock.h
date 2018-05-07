#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>

#ifdef __X86_64__

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

#endif /* __X86_64__ */

#ifdef __I386__

typedef volatile uint32_t lock_t;

#define spinlock_acquire(lock) ({ \
    asm volatile ( \
        "xor eax, eax;" \
        "1: " \
        "lock xchg eax, dword ptr ds:[ebx];" \
        "test eax, eax;" \
        "jz 1b;" \
        : \
        : "b" (lock) \
        : "eax" \
    ); \
})

#define spinlock_test_and_acquire(lock) ({ \
    lock_t ret; \
    asm volatile ( \
        "xor eax, eax;" \
        "lock xchg eax, dword ptr ds:[ebx];" \
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
        "lock xchg eax, dword ptr ds:[ebx];" \
        : \
        : "b" (lock) \
        : "eax" \
    ); \
})

#endif /* __I386__ */

#endif
