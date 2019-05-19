#include <sys/idt.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <sys/exceptions.h>
#include <sys/irq.h>
#include <sys/ipi.h>

static struct idt_entry_t idt[256];

void init_idt(void) {
    /* Register all interrupts */
    for (size_t i = 0; i < 256; i++)
        register_interrupt_handler(i, isr_handler_addresses[i], 0, 0x8e);

    /* Exception handlers */
    register_interrupt_handler(0x0, exc_div0_handler, 0, 0x8e);
    register_interrupt_handler(0x1, exc_debug_handler, 0, 0x8e);
    register_interrupt_handler(0x2, exc_nmi_handler, 0, 0x8e);
    register_interrupt_handler(0x3, exc_breakpoint_handler, 0, 0x8e);
    register_interrupt_handler(0x4, exc_overflow_handler, 0, 0x8e);
    register_interrupt_handler(0x5, exc_bound_range_handler, 0, 0x8e);
    register_interrupt_handler(0x6, exc_inv_opcode_handler, 0, 0x8e);
    register_interrupt_handler(0x7, exc_no_dev_handler, 0, 0x8e);
    register_interrupt_handler(0x8, exc_double_fault_handler, 1, 0x8e);
    register_interrupt_handler(0xa, exc_inv_tss_handler, 0, 0x8e);
    register_interrupt_handler(0xb, exc_no_segment_handler, 0, 0x8e);
    register_interrupt_handler(0xc, exc_ss_fault_handler, 0, 0x8e);
    register_interrupt_handler(0xd, exc_gpf_handler, 0, 0x8e);
    register_interrupt_handler(0xe, exc_page_fault_handler, 0, 0x8e);
    register_interrupt_handler(0x10, exc_x87_fp_handler, 0, 0x8e);
    register_interrupt_handler(0x11, exc_alignment_check_handler, 0, 0x8e);
    register_interrupt_handler(0x12, exc_machine_check_handler, 0, 0x8e);
    register_interrupt_handler(0x13, exc_simd_fp_handler, 0, 0x8e);
    register_interrupt_handler(0x14, exc_virt_handler, 0, 0x8e);
    /* 0x15 .. 0x1d resv. */
    register_interrupt_handler(0x1e, exc_security_handler, 0, 0x8e);

    register_interrupt_handler(0x20, irq0_handler, 0, 0x8e);
    register_interrupt_handler(0x21, irq1_handler, 0, 0x8e);

    /* Inter-processor interrupts */
    register_interrupt_handler(IPI_ABORT, ipi_abort, 1, 0x8e);
    register_interrupt_handler(IPI_RESCHED, ipi_resched, 1, 0x8e);
    register_interrupt_handler(IPI_ABORTEXEC, ipi_abortexec, 1, 0x8e);

    /* Register a bunch of Local APIC NMI handlers */
    for (size_t i = 0; i < 16; i++)
        register_interrupt_handler(0x90 + i, apic_nmi, 1, 0x8e);

    /* Register dummy legacy PIC handlers */
    for (size_t i = 0; i < 8; i++)
        register_interrupt_handler(0xa0 + i, pic0_generic, 1, 0x8e);
    for (size_t i = 0; i < 8; i++)
        register_interrupt_handler(0xa8 + i, pic1_generic, 1, 0x8e);

    register_interrupt_handler(0xff, apic_spurious, 1, 0x8e);

    struct idt_ptr_t idt_ptr = {
        sizeof(idt) - 1,
        (uint64_t)idt
    };

    asm volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );
}

int register_isr(size_t vec, void (**functions)(int, struct regs_t *),
                 size_t count, uint8_t ist, uint8_t type) {
    if (!isr_function_addresses[vec][0])
        isr_function_addresses[vec][0] = (void *)1;
    size_t current_available_vector = (size_t)isr_function_addresses[vec][0];
    for (size_t i = 0; i < count; i++)
        isr_function_addresses[vec][current_available_vector++] = (void *)functions[i];
    isr_function_addresses[vec][0] = (void *)current_available_vector;
    idt[vec].ist = ist;
    idt[vec].type_attr = type;
    return 0;
}

int register_interrupt_handler(size_t vec, void (*handler)(void), uint8_t ist, uint8_t type) {
    uint64_t p = (uint64_t)handler;

    idt[vec].offset_lo = (uint16_t)p;
    idt[vec].selector = 0x08;
    idt[vec].ist = ist;
    idt[vec].type_attr = type;
    idt[vec].offset_mid = (uint16_t)(p >> 16);
    idt[vec].offset_hi = (uint32_t)(p >> 32);
    idt[vec].zero = 0;

    return 0;
}
