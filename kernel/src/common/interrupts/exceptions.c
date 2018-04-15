#include <exceptions.h>
#include <klib.h>
#include <panic.h>

/* FIXME: For exceptions that are recoverable, we should kill the offending process. However,
 * if we are the kernel (as we will be when an exception occurs for the foreseeable future,
 * we should just panic (except for breakpoints) */

/* TODO pass exception error codes as debug info if possible */

void page_fault_handler(void) {
    uint64_t faulting_addr;

    asm volatile("mov %0, cr2": "=r"(faulting_addr));

    panic("CPU exception: Page fault!", 0x1, faulting_addr);
}

void div0_handler(void) {
    panic("CPU exception: Divide-by-zero!", 0x1, 0);
}

void debug_handler(void) {
    panic("CPU exception: Debug exception.", 0x0, 0);
}

void nmi_handler(void) {
    panic("CPU exception: Non-maskable interrupt. Please check your hardware!", 0x2, 0); 
}

void breakpoint_handler(void) {
    kprint(KPRN_DBG, "CPU exception: Breakpoint");
}

void overflow_handler(void) {
    panic("CPU exception: Overflow", 0x0, 0);
}

void bound_range_handler(void) {
    panic("CPU exception: Bound range exceeded!", 0x1, 0);
}

void inv_opcode_handler(void) {
    panic("CPU exception: Invalid opcode!", 0x1, 0);
}

void no_dev_handler(void) {
    panic("CPU exception: Device not found!", 0x1, 0);
}

void double_fault_handler(void) {
    panic("CPU exception: Double fault!", 0x2, 0);
}

void inv_tss_handler(void) {
    panic("CPU exception: Invalid TSS!", 0x1, 0);
}

void no_segment_handler(void) {
    panic("CPU exception: Segment not present!", 0x1, 0);
}

void ss_fault_handler(void) {
    panic("CPU exception: Stack segment fault!", 0x1, 0);
}

void gpf_handler(void) {
    panic("CPU exception: General protection fault!", 0x1, 0);
}

void x87_fp_handler(void) {
    panic("CPU exception: x87 floating-point exception!", 0x1, 0);
}

void alignment_check_handler(void) {
    panic("CPU exception: Alignment check!", 0x1, 0);
}

void machine_check_handler(void) {
    panic("CPU exception: Machine check! Possible internal processor error.", 0x2, 0);
}

void simd_fp_handler(void) {
    panic("CPU exception: SIMD floating-point exception!", 0x1, 0);
}

void virt_handler(void) {
    panic("CPU exception: Virtualization exception!", 0x1, 0);
}

void security_handler(void) {
    panic("CPU exception: Security exception!", 0x1, 0);
}
