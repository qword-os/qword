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
    for (int i = 0; i < smp_cpu_count; i++) {
        if (i == current_cpu)
            continue;
        lapic_send_ipi(i, IPI_ABORT);
    }

    kprint(KPRN_PANIC, "KERNEL PANIC ON CPU #%U", current_cpu);
    kprint(KPRN_PANIC, "%s", msg);
    kprint(KPRN_PANIC, "Error code: %X", error_code);
    kprint(KPRN_PANIC, "Debug info: %X", debug_info);
    kprint(KPRN_PANIC, "Current task: %d", cpu_locals[current_cpu].current_task);
    kprint(KPRN_PANIC, "Current process: %d", cpu_locals[current_cpu].current_process);
    kprint(KPRN_PANIC, "Current thread: %d", cpu_locals[current_cpu].current_thread);

    kprint(KPRN_PANIC, "System halted");

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
    for (int i = 0; i < smp_cpu_count; i++) {
        if (i == current_cpu)
            continue;
        lapic_send_ipi(i, IPI_ABORT);
    }

    kprint(KPRN_PANIC, "EXCEPTION ON CPU #%U", current_cpu);
    kprint(KPRN_PANIC, "%s", msg);
    kprint(KPRN_PANIC, "Error code: %X", error_code);
    kprint(KPRN_PANIC, "Debug info: %X", debug_info);
    kprint(KPRN_PANIC, "Faulting instruction at: %X:%X", cs, ip);
    kprint(KPRN_PANIC, "Current task: %d", cpu_locals[current_cpu].current_task);
    kprint(KPRN_PANIC, "Current process: %d", cpu_locals[current_cpu].current_process);
    kprint(KPRN_PANIC, "Current thread: %d", cpu_locals[current_cpu].current_thread);

    kprint(KPRN_PANIC, "System halted");

    asm volatile(
            "1:"
            "hlt;"
            "jmp 1b;"
    );
}
