#include <panic.h>
#include <klib.h>

void panic(const char *msg, int err_code, uint64_t debug_info) {
    asm volatile("cli");
    kprint(KPRN_ERR, "KERNEL PANIC:");
    kprint(KPRN_ERR, "%s, error code: %X", msg, err_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);
    halt();
}

void halt(void) {
    asm volatile(       \
            "1:"        \
            "cli;"      \
            "hlt;"      \
            "jmp 1b;"   \
    );
}
