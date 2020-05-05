#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <proc/task.h>
#include <sys/cpu.h>
#include <lib/klib.h>

#define panic_unless(c) ({ \
    if(!(c)) \
        panic("panic_unless(" #c ") triggered in " \
              __FILE__ ":" expand_stringify(__LINE__), 0, 0, NULL); \
})

#define panic_if(c) ({ \
    if((c)) \
        panic("panic_if(" #c ") triggered in " \
              __FILE__ ":" expand_stringify(__LINE__), 0, 0, NULL); \
})

void panic(struct regs_t *regs, int print_trace, const char *fmt, ...);

#endif
