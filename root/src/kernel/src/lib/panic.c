#include <stddef.h>
#include <panic.h>
#include <klib.h>
#include <lock.h>
#include <trace.h>
#include <smp.h>

static lock_t panic_lock = 1;

void panic(const char *msg, size_t error_code, size_t debug_info) {
    asm volatile ("cli");

    spinlock_acquire(&panic_lock);

    /* TODO: should send an abort IPI to all other APs */

    kprint(KPRN_ERR, "KERNEL PANIC ON CPU #%U", current_cpu);
    kprint(KPRN_ERR, "%s", msg);
    kprint(KPRN_ERR, "Error code: %X", error_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);

    print_stacktrace(KPRN_ERR);

    kprint(KPRN_ERR, "System halted");

    /* Never release the spinlock */

    asm volatile (
            "1:"
            "hlt;"
            "jmp 1b;"
    );
}

static lock_t kexcept_lock = 1;

void kexcept(const char *msg, size_t cs, size_t ip, size_t error_code, size_t debug_info) {
    asm volatile ("cli");

    spinlock_acquire(&kexcept_lock);

    kprint(KPRN_ERR, "EXCEPTION ON CPU #%U", current_cpu);
    kprint(KPRN_ERR, "%s", msg);
    kprint(KPRN_ERR, "Error code: %X", error_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);
    kprint(KPRN_ERR, "Faulting instruction at: %X:%X", cs, ip);

    print_stacktrace(KPRN_ERR);

    kprint(KPRN_ERR, "System halted");

    asm volatile(
            "1:"
            "hlt;"
            "jmp 1b;"            
    );
}
