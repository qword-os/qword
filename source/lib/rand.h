#ifndef __RAND_H__
#define __RAND_H__

#include <stdint.h>

void init_rand(void);
uint32_t rand32(void);
uint64_t rand64(void);
void srand(uint32_t);

#define rdrand_supported ({ \
    int ret; \
    asm volatile ( \
        "cpuid;" \
        "bt ecx, 30;" \
        : "=@ccc" (ret) \
        : "a" (1), "c" (0) \
    ); \
    ret; \
})

#define rdrand(type) ({ \
    type ret; \
    asm volatile ( \
        "1: " \
        "rdrand %0;" \
        "jnc 1b;" \
        : "=r" (ret) \
        : \
        : "cc" \
    ); \
    ret; \
})

#define rdtsc(type) ({ \
    type ret; \
    asm volatile ( \
        "rdtsc;" \
        "shl rdx, 32;" \
        "or rax, rdx;" \
        : "=a" (ret) \
        : \
        : "rdx" \
    ); \
    ret; \
})

#endif
