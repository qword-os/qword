#include <stdint.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <acpi/madt.h>
#include <panic.h>
#include <smp.h>
#include <time.h>
#include <mm.h>
#include <task.h>

#define CPU_STACK_SIZE (8192+8192)

int smp_cpu_count = 1;

typedef struct {
    uint32_t unused __attribute__((aligned(16)));
    uint64_t sp;
    uint32_t entries[23];
} __attribute__((packed)) tss_t;

static size_t cpu_stack_top = KERNEL_PHYS_OFFSET + 0xeffff0;

cpu_local_t cpu_locals[MAX_CPUS];
static tss_t cpu_tss[MAX_CPUS] __attribute__((aligned(16)));

static void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "smp: Started up AP #%u", smp_get_cpu_number());
    kprint(KPRN_INFO, "smp: AP #%u kernel stack top: %X", smp_get_cpu_number(), smp_get_cpu_kernel_stack());
    
    /* halt and catch fire */
    for (;;) { asm volatile ("cli; hlt"); }
}

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    if (cpu_number == MAX_CPUS) {
        panic("smp: CPU limit exceeded", smp_cpu_count, 0);
    }

    /* create CPU local struct */
    cpu_local_t *cpu_local = &cpu_locals[cpu_number];

    kprint(KPRN_DBG, "Setting up CPU local struct.");
    cpu_local->cpu_number = cpu_number;
    cpu_local->kernel_stack = cpu_stack_top;
    cpu_local->idle = 1;
    cpu_local->current_process = 0;
    cpu_local->current_thread = 0;
    cpu_local->should_ts = 0;
    cpu_local->idle_time = 0;
    cpu_local->load = 0;
    if ((cpu_local->run_queue = kalloc(MAX_THREADS * sizeof(thread_identifier_t))) == 0) {
        panic("smp: Failed to allocate thread array for CPU with number: ", cpu_number, 0);
    }

    /* prepare TSS */
    kprint(KPRN_DBG, "Preparing TSS.");
    tss_t *tss = &cpu_tss[cpu_number];

    tss->sp = (uint64_t)cpu_stack_top;
    
    kprint(KPRN_DBG, "Preparing smp trampoline...");
    void *trampoline = smp_prepare_trampoline(ap_kernel_entry, (void *)kernel_pagemap.pagemap,
                                (void *)cpu_stack_top, cpu_local, tss);
    
    kprint(KPRN_DBG, "Sending init IPI");
    /* Send the INIT IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4500);
    kprint(KPRN_DBG, "Waiting 10ms");
    /* wait 10ms */
    ksleep(10);
    kprint(KPRN_DBG, "Sending startup IPI");
    /* Send the Startup IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
    /* wait 1ms */
    ksleep(1);

    if (smp_check_ap_flag()) {
        goto success;
    } else {
        kprint(KPRN_DBG, "Sending startup IPI again...");
        /* Send the Startup IPI again */
        lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
        lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
        /* wait 1s */
        ksleep(1000);
        if (smp_check_ap_flag())
            goto success;
        else
            return -1;
    }

success:
    cpu_stack_top -= CPU_STACK_SIZE;
    return 0;
}

static void init_cpu0(void) {
    /* create CPU 0 local struct */
    cpu_local_t *cpu_local = &cpu_locals[0];

    cpu_local->cpu_number = 0;
    cpu_local->kernel_stack = cpu_stack_top;
    cpu_local->idle = 0;
    cpu_local->current_process = 0;
    cpu_local->current_thread = 0;
    cpu_local->should_ts = 0;
    cpu_local->idle_time = 0;
    cpu_local->load = 0;
    if ((cpu_local->run_queue = kalloc(sizeof(thread_t) * MAX_THREADS)) == 0) {
        panic("smp: Failed to allocate thread array for CPU0", 0, 0);
    }

    tss_t *tss = &cpu_tss[0];

    tss->sp = (uint64_t)cpu_stack_top;

    smp_init_cpu0_local(cpu_local, tss);

    cpu_stack_top -= CPU_STACK_SIZE;

    return;
}

void init_smp(void) {
    /* prepare CPU 0 first */
    init_cpu0();

    /* start up the APs and jump them into the kernel */
    for (size_t i = 1; i < madt_local_apic_ptr; i++) {
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

    return;
    }
