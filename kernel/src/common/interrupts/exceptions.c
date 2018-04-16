#include <stddef.h>
#include <exceptions.h>
#include <klib.h>
#include <panic.h>

/* FIXME: For exceptions that are recoverable, we should kill the offending process. However,
 * if we are the kernel (as we will be when an exception occurs for the foreseeable future,
 * we should just panic (except for breakpoints) */

/* TODO pass exception error codes as debug info if possible */


void div0_handler(size_t cs, size_t ip) {
    kexcept("Page fault!", cs, ip, 0, 0);
}

void debug_handler(size_t cs, size_t ip) {
    kexcept("Debug exception!", cs, ip, 0, 0);
}

void nmi_handler(size_t cs, size_t ip) {
    kexcept("Non-maskable interrupt, please check your hardware!", cs, ip, 0, 0);
}

void breakpoint_handler(size_t cs, size_t ip) {
    kexcept("Breakpoint exception!", cs, ip, 0, 0);
}

void overflow_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Overflow", cs, ip, 0, 0);
}

void bound_range_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Bound range exceeded!", cs, ip, 0, 0);
}

void inv_opcode_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Invalid opcode!", cs, ip, 0, 0);
}

void no_dev_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Device not found!", cs, ip, 0, 0);
}

void double_fault_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Double fault!", cs, ip, error_code, 0);
}

void inv_tss_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Invalid TSS!", cs, ip, error_code, 0);
}

void no_segment_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Segment not present!", cs, ip, error_code, 0);
}

void ss_fault_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Stack segment fault!", cs, ip, error_code, 0);
}

void gpf_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: General protection fault!", cs, ip, error_code, 0);
}

void page_fault_handler(size_t cs, size_t ip, size_t error_code) {
    size_t faulting_addr;

    asm volatile (
        "mov %0, cr2"
        : "=r" (faulting_addr)
    );

    kexcept("CPU exception: Page fault!", cs, ip, error_code, faulting_addr);
}

void x87_fp_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: x87 floating-point exception!", cs, ip, 0, 0);
}

void alignment_check_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Alignment check!", cs, ip, error_code, 0);
}

void machine_check_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Machine check! Possible internal processor error.", cs, ip, 0, 0);
}

void simd_fp_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: SIMD floating-point exception!", cs, ip, 0, 0);
}

void virt_handler(size_t cs, size_t ip) {
    kexcept("CPU exception: Virtualization exception!", cs, ip, 0, 0);
}

void security_handler(size_t cs, size_t ip, size_t error_code) {
    kexcept("CPU exception: Security exception!", cs, ip, error_code, 0);
}
