#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>
#include <stddef.h>

#define STRINGIFY(x) #x
#define EXPAND_STRINGIFY(x) STRINGIFY(x)

#define PANIC_UNLESS(c) \
    do { \
        if(!(c)) \
            panic("PANIC_UNLESS(" #c ") triggered in " \
                    __FILE__ ":" EXPAND_STRINGIFY(__LINE__), 0, 0); \
    } while(0)

void panic(const char *, uint64_t, uint64_t);
void kexcept(const char *, size_t, size_t, size_t, size_t);

#endif
