#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/panic.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <sys/trace.h>
#include <proc/task.h>
#include <sys/apic.h>

static int panic_lock = 0;

void panic(const char *msg, size_t debug_info, size_t error_code, struct regs_t *regs) {
    asm volatile ("cli");
    panic2(regs, 1, "%s\nDebug info: %16X\nError code: %16X", msg, debug_info, error_code);
}

void panic2(struct regs_t *regs, int print_trace, const char *fmt, ...) {
    asm volatile ("cli");

    // Be the only one to panic no matter what, no double panics!
    if (locked_write(int, &panic_lock, 1)) {
        for (;;) {
            asm volatile ("hlt");
        }
    }

    int _current_cpu = current_cpu;
    struct cpu_local_t *cpu_local = &cpu_locals[current_cpu];

    // Send an abort IPI to all other APs
    if (smp_ready) {
        for (int i = 0; i < smp_cpu_count; i++) {
            if (i == _current_cpu)
                continue;
            locked_write(size_t, &cpu_locals[i].ipi_abort_received, 0);
            lapic_send_ipi(i, IPI_ABORT);
            while (!locked_read(size_t, &cpu_locals[i].ipi_abort_received));
        }
    }

    va_list args;
    va_start(args, fmt);
    if (smp_ready)
        kprint(KPRN_PANIC, "KERNEL PANIC ON CPU #%d", _current_cpu);
    else
        kprint(KPRN_PANIC, "KERNEL PANIC ON THE BSP");
    kvprint(KPRN_PANIC, fmt, args);
    va_end(args);

    if (regs) {
        kprint(KPRN_PANIC, "CPU status at fault:");
        kprint(KPRN_PANIC, "  RAX: %16X  RBX: %16X  RCX: %16X  RDX: %16X",
                           regs->rax,
                           regs->rbx,
                           regs->rcx,
                           regs->rdx);
        kprint(KPRN_PANIC, "  RSI: %16X  RDI: %16X  RBP: %16X  RSP: %16X",
                           regs->rsi,
                           regs->rdi,
                           regs->rbp,
                           regs->rsp);
        kprint(KPRN_PANIC, "  R8:  %16X  R9:  %16X  R10: %16X  R11: %16X",
                           regs->r8,
                           regs->r9,
                           regs->r10,
                           regs->r11);
        kprint(KPRN_PANIC, "  R12: %16X  R13: %16X  R14: %16X  R15: %16X",
                           regs->r12,
                           regs->r13,
                           regs->r14,
                           regs->r15);
        kprint(KPRN_PANIC, "  RFLAGS: %16X", regs->rflags);
        kprint(KPRN_PANIC, "  RIP:    %16X", regs->rip);
        kprint(KPRN_PANIC, "  CS:  %4X    SS:  %4X",
                           regs->cs,
                           regs->ss);
        kprint(KPRN_PANIC, "  CR2: %16X", read_cr("2"));
    }

    if (smp_ready) {
        kprint(KPRN_PANIC, "Current task:    %d", cpu_local->current_task);
        kprint(KPRN_PANIC, "Current process: %d", cpu_local->current_process);
        kprint(KPRN_PANIC, "Current thread:  %d", cpu_local->current_thread);
    } else {
        kprint(KPRN_PANIC, "SMP and scheduler were NOT initialiased at panic.");
    }

    if (print_trace)
        print_stacktrace(KPRN_PANIC);

    kprint(KPRN_PANIC, "System halted");

    for (;;) {
        asm volatile ("hlt");
    }
}
