#include <stddef.h>
#include <symlist.h>
#include <lib/klib.h>
#include <sys/trace.h>

char *trace_address(size_t *off, size_t addr) {
    for (size_t i = 0; ; i++) {
        if (symlist[i].addr >= addr) {
            *off = addr - symlist[i-1].addr;
            return symlist[i-1].name;
        }
    }
}

void print_stacktrace(int type) {
    size_t *base_ptr;
    asm volatile (
        "mov %0, rbp;"
        : "=r"(base_ptr)
    );
    kprint(type, "Stacktrace:");
    for (;;) {
        size_t old_bp = base_ptr[0];
        size_t ret_addr = base_ptr[1];
        size_t off;
        if (!ret_addr)
            break;
        char *name = trace_address(&off, ret_addr);
        kprint(type, "  [%16X] <%s+%X>", ret_addr, name, off);
        if (!old_bp)
            break;
        base_ptr = (void*)old_bp;
    }
}
