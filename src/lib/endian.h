#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include <stdint.h>

#define bswap16(x) ({ \
    uint16_t ret = x; \
    asm volatile (    \
        "xchg ah, al" \
        : "+a"(ret)   \
    );                \
    ret;              \
})

#define bswap32(x) ({ \
    uint32_t ret = x; \
    asm volatile (    \
        "bswap %0"    \
        : "+r"(ret)   \
    );                \
    ret;              \
})

#define bswap64(x) ({ \
    uint64_t ret = x; \
    asm volatile (    \
        "bswap %0"    \
        : "+r"(ret)   \
    );                \
    ret;              \
})

#endif
