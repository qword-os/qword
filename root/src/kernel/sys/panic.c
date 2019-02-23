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

void panic(const char *msg, size_t debug_info, size_t error_code, struct regs_t *regs) {
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

    if (smp_ready)
        kprint(KPRN_PANIC, "KERNEL PANIC ON CPU #%d", _current_cpu);
    else
        kprint(KPRN_PANIC, "KERNEL PANIC ON THE BSP");

    kprint(KPRN_PANIC, "%s", msg);
    kprint(KPRN_PANIC, "Debug info: %X", debug_info);
    kprint(KPRN_PANIC, "Error code: %X", error_code);

    if (regs) {
        kprint(KPRN_PANIC, "CPU status at fault:");
        kprint(KPRN_PANIC, "RAX:    %X", regs->rax);
        kprint(KPRN_PANIC, "RBX:    %X", regs->rbx);
        kprint(KPRN_PANIC, "RCX:    %X", regs->rcx);
        kprint(KPRN_PANIC, "RDX:    %X", regs->rdx);
        kprint(KPRN_PANIC, "RSI:    %X", regs->rsi);
        kprint(KPRN_PANIC, "RDI:    %X", regs->rdi);
        kprint(KPRN_PANIC, "RBP:    %X", regs->rbp);
        kprint(KPRN_PANIC, "RSP:    %X", regs->rsp);
        kprint(KPRN_PANIC, "R8:     %X", regs->r8);
        kprint(KPRN_PANIC, "R9:     %X", regs->r9);
        kprint(KPRN_PANIC, "R10:    %X", regs->r10);
        kprint(KPRN_PANIC, "R11:    %X", regs->r11);
        kprint(KPRN_PANIC, "R12:    %X", regs->r12);
        kprint(KPRN_PANIC, "R13:    %X", regs->r13);
        kprint(KPRN_PANIC, "R14:    %X", regs->r14);
        kprint(KPRN_PANIC, "R15:    %X", regs->r15);
        kprint(KPRN_PANIC, "RFLAGS: %X", regs->rflags);
        kprint(KPRN_PANIC, "RIP:    %X", regs->rip);
        kprint(KPRN_PANIC, "CS:     %X", regs->cs);
        kprint(KPRN_PANIC, "SS:     %X", regs->ss);
        kprint(KPRN_PANIC, "CR2:    %X", read_cr2());
    }

    if (smp_ready) {
        kprint(KPRN_PANIC, "Current task: %d", cpu_local->current_task);
        kprint(KPRN_PANIC, "Current process: %d", cpu_local->current_process);
        kprint(KPRN_PANIC, "Current thread: %d", cpu_local->current_thread);
    } else {
        kprint(KPRN_PANIC, "SMP and scheduler were NOT initialiased at panic.");
    }

    kprint(KPRN_PANIC, "System halted");

    for (;;) {
        asm volatile ("hlt");
    }
}
