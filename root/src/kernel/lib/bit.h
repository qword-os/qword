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

#endif
