#include <stddef.h>
#include <sys/exceptions.h>
#include <sys/panic.h>
#include <proc/task.h>
#include <mm/mm.h>
#include <lib/klib.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <lib/signal.h>

#define EXC_DIV0 0x0
#define EXC_DEBUG 0x1
#define EXC_NMI 0x2
#define EXC_BREAKPOINT 0x3
#define EXC_OVERFLOW 0x4
#define EXC_BOUND 0x5
#define EXC_INVOPCODE 0x6
#define EXC_NODEV 0x7
#define EXC_DBFAULT 0x8
#define EXC_INVTSS 0xa
#define EXC_NOSEGMENT 0xb
#define EXC_SSFAULT 0xc
#define EXC_GPF 0xd
#define EXC_PAGEFAULT 0xe
#define EXC_FP 0x10
#define EXC_ALIGN 0x11
#define EXC_MACHINECHK 0x12
#define EXC_SIMD 0x13
#define EXC_VIRT 0x14
#define EXC_SECURITY 0x1e

static const char *exception_names[] = {
    "Division by 0",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "???",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "???",
    "x87 exception",
    "Alignment check",
    "Machine check",
    "SIMD exception",
    "Virtualisation",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "???",
    "Security"
};

void exception_handler(int exception, struct regs_t *regs, size_t error_code) {

    if (regs->cs == 0x23) {
        // userspace
        switch (exception) {
            case 0:
            case 16:
            case 19:
                asm volatile ("sti");
                kill(cpu_locals[current_cpu].current_process, SIGFPE);
                break;
            case 6:
                asm volatile ("sti");
                kill(cpu_locals[current_cpu].current_process, SIGILL);
                break;
            case 13:
            case 14:
                asm volatile ("sti");
                kill(cpu_locals[current_cpu].current_process, SIGSEGV);
                break;
            default:
                break;
        }
    }

    // this is a kernel exception/unhandled exception, ouch!
    panic(exception_names[exception], exception, error_code, regs);
}
