#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <sys/apic.h>
#include <acpi/acpi.h>
#include <acpi/madt.h>
#include <sys/panic.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <lib/time.h>
#include <mm/mm.h>
#include <proc/task.h>

#define CPU_STACK_SIZE 16384

int smp_ready = 0;

/* External assembly routines */
void smp_init_cpu0_local(void *, void *);
void *smp_prepare_trampoline(void *, void *, void *, void *, void *);
int smp_check_ap_flag(void);

int smp_cpu_count = 1;

struct tss_t {
    uint32_t unused0 __attribute__((aligned(16)));
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint32_t iopb_offset;
} __attribute__((packed));

struct cpu_local_t cpu_locals[MAX_CPUS];
static struct tss_t cpu_tss[MAX_CPUS] __attribute__((aligned(16)));

struct stack_t {
    uint8_t guard_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
    uint8_t stack[CPU_STACK_SIZE] __attribute__((aligned(PAGE_SIZE)));
};

static struct stack_t cpu_stacks[MAX_CPUS] __attribute__((aligned(PAGE_SIZE)));

static void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "smp: Started up AP #%u", current_cpu);

    /* Enable this AP's local APIC */
    lapic_enable();

    /* Enable interrupts */
    asm volatile ("sti");

    /* Wait for some job to be scheduled */
    for (;;) asm volatile ("hlt");
}

static inline void setup_cpu_local(int cpu_number, uint8_t lapic_id) {
    /* Set up stack guard page */
    unmap_page(kernel_pagemap, (size_t)&cpu_stacks[cpu_number].guard_page[0]);

    /* Prepare CPU local */
    cpu_locals[cpu_number].cpu_number = cpu_number;
    cpu_locals[cpu_number].kernel_stack = (size_t)&cpu_stacks[cpu_number].stack[CPU_STACK_SIZE];
    cpu_locals[cpu_number].current_process = -1;
    cpu_locals[cpu_number].current_thread = -1;
    cpu_locals[cpu_number].current_task = -1;
    cpu_locals[cpu_number].lapic_id = lapic_id;

    /* Prepare TSS */
    cpu_tss[cpu_number].rsp0 = (uint64_t)&cpu_stacks[cpu_number].stack[CPU_STACK_SIZE];
    cpu_tss[cpu_number].ist1 = (uint64_t)&cpu_stacks[cpu_number].stack[CPU_STACK_SIZE];

    return;
}

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    if (cpu_number == MAX_CPUS) {
        panic("smp: CPU limit exceeded", smp_cpu_count, 0, NULL);
    }

    setup_cpu_local(cpu_number, target_apic_id);

    struct cpu_local_t *cpu_local = &cpu_locals[cpu_number];
    struct tss_t *tss = &cpu_tss[cpu_number];
    uint8_t *stack = &cpu_stacks[cpu_number].stack[CPU_STACK_SIZE];

    void *trampoline = smp_prepare_trampoline(ap_kernel_entry, (void *)kernel_pagemap->pml4,
                                              stack, cpu_local, tss);

    /* Send the INIT IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x500);
    /* wait 10ms */
    ksleep(10);
    /* Send the Startup IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x600 | (uint32_t)(size_t)trampoline);

    for (int i = 0; i < 1000; i++) {
        ksleep(1);
        if (smp_check_ap_flag())
            return 0;
    }
    return -1;
}

static void init_cpu0(void) {
    setup_cpu_local(0, 0);

    struct cpu_local_t *cpu_local = &cpu_locals[0];
    struct tss_t *tss = &cpu_tss[0];

    smp_init_cpu0_local(cpu_local, tss);

    return;
}

void init_smp(void) {
    /* prepare CPU 0 first */
    init_cpu0();

    /* start up the APs and jump them into the kernel */
    for (size_t i = 1; i < madt_local_apic_i; i++) {
        /*
         * In practice some "dead" cores have a lapic == 0, that as we know is
         * the one of AP #0. This cores are meant to be ignored, at least for
         * SMP, if not the system will freeze on the INIT IPI sending.
         * Tested on an AMD Ryzen 5 2400G (8) @ 3
         */
        if (!madt_local_apics[i]->apic_id) {
            kprint(KPRN_INFO, "smp: Theoretical AP #%u ignored", i);
            continue;
        }

        kprint(KPRN_INFO, "smp: Starting up AP #%u", i);

        if (start_ap(madt_local_apics[i]->apic_id, smp_cpu_count)) {
            kprint(KPRN_ERR, "smp: Failed to start AP #%u", i);
            continue;
        }

        smp_cpu_count++;
        /* wait a bit */
        ksleep(10);
    }

    kprint(KPRN_INFO, "smp: Total CPU count: %u", smp_cpu_count);

    smp_ready = 1;

    return;
}
