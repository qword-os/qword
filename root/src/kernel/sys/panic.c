#include <stddef.h>
#include <stdint.h>
#include <sys/panic.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <proc/task.h>
#include <sys/apic.h>

static int panic_lock = 0;

void panic(const char *msg, size_t error_code, size_t debug_info) {
    asm volatile ("cli");

    // Be the only one to panic no matter what, no double panics!
    if (locked_write(int, &panic_lock, 1)) {
        for (;;) {
            asm volatile ("hlt");
        }
    }

    // Send an abort IPI to all other APs
    if (smp_ready) {
        for (int i = 0; i < smp_cpu_count; i++) {
            if (i == current_cpu)
                continue;
            lapic_send_ipi(i, IPI_ABORT);
        }
    }

    if (smp_ready)
        kprint(KPRN_PANIC, "KERNEL PANIC ON CPU #%d", current_cpu);
    else
        kprint(KPRN_PANIC, "KERNEL PANIC ON THE BSP");

    kprint(KPRN_PANIC, "%s", msg);
    kprint(KPRN_PANIC, "Error code: %X", error_code);
    kprint(KPRN_PANIC, "Debug info: %X", debug_info);

    if (smp_ready) {
        kprint(KPRN_PANIC, "Current task: %d", cpu_locals[current_cpu].current_task);
        kprint(KPRN_PANIC, "Current process: %d", cpu_locals[current_cpu].current_process);
        kprint(KPRN_PANIC, "Current thread: %d", cpu_locals[current_cpu].current_thread);
    } else {
        kprint(KPRN_PANIC, "SMP and scheduler were NOT initialiased at panic.");
    }

    kprint(KPRN_PANIC, "System halted");

    for (;;) {
        asm volatile ("hlt");
    }
}
