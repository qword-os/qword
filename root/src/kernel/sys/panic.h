#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>
#include <stddef.h>
#include <proc/task.h>

#define stringify(x) #x
#define expand_stringify(x) stringify(x)

#define panic_unless(c) \
    do { \
        if(!(c)) \
            panic("panic_unless(" #c ") triggered in " \
                    __FILE__ ":" expand_stringify(__LINE__), 0, 0); \
    } while(0)

void panic(const char *, uint64_t, uint64_t);

#endif
