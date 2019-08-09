#ifndef __BIT_H__
#define __BIT_H__

#define bit_test(var, offset) ({ \
    int __ret; \
    asm volatile ( \
        "bt %1, %2;" \
        : "=@ccc" (__ret) \
        : "r" ((uint32_t)(var)), "r" ((uint32_t)(offset)) \
    ); \
    __ret; \
})

#define test_bit(base, offset) ({ \
    int ret; \
    asm volatile ( \
        "bt [%1], %2;" \
        : "=@ccc" (ret) \
        : "r" (base), "r" (offset) \
        : "memory" \
    ); \
    ret; \
})

#define set_bit(base, offset) ({ \
    int ret; \
    asm volatile ( \
        "bts [%1], %2;" \
        : "=@ccc" (ret) \
        : "r" (base), "r" (offset) \
        : "memory" \
    ); \
    ret; \
})

#define reset_bit(base, offset) ({ \
    int ret; \
    asm volatile ( \
        "btr [%1], %2;" \
        : "=@ccc" (ret) \
        : "r" (base), "r" (offset) \
        : "memory" \
    ); \
    ret; \
})

#endif
