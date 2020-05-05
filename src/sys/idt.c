#include <sys/idt.h>
#include <sys/exceptions.h>
#include <sys/isrs.h>
#include <sys/ipi.h>
#include <lib/lock.h>

static lock_t get_empty_int_lock = new_lock;
static int free_int_vect_base = 0x80;
static const int free_int_vect_limit = 0xa0;

int get_empty_int_vector(void) {
    spinlock_acquire(&get_empty_int_lock);
    int ret;
    if (free_int_vect_base == free_int_vect_limit)
        ret = -1;
    else
        ret = free_int_vect_base++;
    spinlock_release(&get_empty_int_lock);
    return ret;
}

struct idt_entry_t {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
};

struct idt_ptr_t {
    uint16_t size;
    /* Start address */
    uint64_t address;
} __attribute((packed));

static struct idt_entry_t idt[256];
extern void *int_thunks[];

static int register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type) {
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

void init_idt(void) {
    /* Register all interrupts */
    for (size_t i = 0; i < 256; i++)
        register_interrupt_handler(i, int_thunks[i], 0, 0x8e);

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

    /* Inter-processor interrupts */
    register_interrupt_handler(IPI_ABORT, ipi_abort, 1, 0x8e);
    register_interrupt_handler(IPI_RESCHED, ipi_resched, 1, 0x8e);
    register_interrupt_handler(IPI_ABORTEXEC, ipi_abortexec, 1, 0x8e);

    /* Register dummy legacy PIC handlers */
    for (size_t i = 0; i < 8; i++)
        register_interrupt_handler(0xa0 + i, pic0_generic_handler, 1, 0x8e);
    for (size_t i = 0; i < 8; i++)
        register_interrupt_handler(0xa8 + i, pic1_generic_handler, 1, 0x8e);

    /* Register local APIC NMI handler */
    register_interrupt_handler(0xf0, apic_nmi_handler, 1, 0x8e);

    register_interrupt_handler(0xff, apic_spurious_handler, 1, 0x8e);

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
