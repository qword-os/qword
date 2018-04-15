#include <panic.h>
#include <klib.h>
#include <lock.h>

static lock_t panic_lock = 1;

void panic(const char *msg, uint64_t err_code, uint64_t debug_info) {
    asm volatile ("cli");

    spinlock_acquire(&panic_lock);

    /* TODO: should send an abort IPI to all other APs */

    kprint(KPRN_ERR, "KERNEL PANIC:");
    kprint(KPRN_ERR, "%s, error code: %X", msg, err_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);
    kprint(KPRN_ERR, "System halted");

    /* Never release the spinlock */

    asm volatile (
            "1:"
            "hlt;"
            "jmp 1b;"
    );
}
