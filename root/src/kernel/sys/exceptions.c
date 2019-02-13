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
        asm volatile ("sti");
        switch (exception) {
            case 13:
            case 14:
                kill(cpu_locals[current_cpu].current_process, SIGSEGV);
                break;
            default:
                asm volatile ("cli");
                break;
        }
    }

    // this is a kernel exception/unhandled exception, ouch!

    kprint(KPRN_PANIC, "Exception \"%s\" (int %x)", exception_names[exception], exception);
    kprint(KPRN_PANIC, "Error code: %X", error_code);
    kprint(KPRN_PANIC, "CPU #%d status at fault:", current_cpu);
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
    panic("CPU exception", 0, 0);
}
