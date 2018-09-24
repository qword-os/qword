#include <stddef.h>
#include <stdint.h>
#include <panic.h>
#include <klib.h>
#include <lock.h>
#include <smp.h>
#include <task.h>
#include <apic.h>

static lock_t panic_lock = 1;

void panic(const char *msg, size_t error_code, size_t debug_info) {
    asm volatile ("cli");

    spinlock_acquire(&panic_lock);
    spinlock_test_and_acquire(&scheduler_lock);

    /* Send an abort IPI to all other APs */
    for (int i = 1; i < smp_cpu_count; i++) {
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_ABORT);
    }

    kprint(KPRN_ERR, "KERNEL PANIC ON CPU #%U", current_cpu);
    kprint(KPRN_ERR, "%s", msg);
    kprint(KPRN_ERR, "Error code: %X", error_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);

    kprint(KPRN_ERR, "System halted");

    asm volatile (
            "1:"
            "hlt;"
            "jmp 1b;"
    );
}

void kexcept(const char *msg, size_t cs, size_t ip, size_t error_code, size_t debug_info) {
    asm volatile ("cli");

    spinlock_acquire(&panic_lock);
    spinlock_test_and_acquire(&scheduler_lock);

    /* Send an abort IPI to all other APs */
    for (int i = 1; i < smp_cpu_count; i++) {
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_ABORT);
    }

    kprint(KPRN_ERR, "EXCEPTION ON CPU #%U", current_cpu);
    kprint(KPRN_ERR, "%s", msg);
    kprint(KPRN_ERR, "Error code: %X", error_code);
    kprint(KPRN_ERR, "Debug info: %X", debug_info);
    kprint(KPRN_ERR, "Faulting instruction at: %X:%X", cs, ip);

    kprint(KPRN_ERR, "System halted");

    asm volatile(
            "1:"
            "hlt;"
            "jmp 1b;"            
    );
}
