#include <stdint.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <acpi/madt.h>
#include <panic.h>
#include <smp.h>
#include <time.h>
#include <mm.h>

#define MAX_CPUS 128
#define CPU_STACK_SIZE (8192+8192)

int smp_cpu_count = 1;

#ifdef __X86_64__
    typedef struct {
        uint32_t unused __attribute__((aligned(16)));
        uint64_t sp;
        uint32_t entries[23];
    } __attribute__((packed)) tss_t;
#endif
#ifdef __I386__
    typedef struct {
        uint32_t unused __attribute__((aligned(16)));
        uint32_t sp;
        uint32_t ss;
        uint32_t entries[23];
    } __attribute__((packed)) tss_t;
#endif

#ifdef __X86_64__
    static size_t cpu_stack_top = 0xffffffffc0effff0;
#endif
#ifdef __I386__
    static size_t cpu_stack_top = 0xa0effff0;
#endif

static cpu_local_t cpu_locals[MAX_CPUS];
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

    cpu_local->cpu_number = cpu_number;
    cpu_local->kernel_stack = cpu_stack_top;

    /* prepare TSS */
    tss_t *tss = &cpu_tss[cpu_number];

    #ifdef __X86_64__
        tss->sp = (uint64_t)cpu_stack_top;
    #endif
    #ifdef __I386__
        tss->sp = (uint32_t)cpu_stack_top;
        tss->ss = 0x08;
    #endif

    void *trampoline = smp_prepare_trampoline(ap_kernel_entry, (void *)((size_t)kernel_pagemap.pagemap - KERNEL_PHYS_OFFSET),
                                (void *)cpu_stack_top, cpu_local, tss);

    /* Send the INIT IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4500);
    /* wait 10ms */
    ksleep(10);
    /* Send the Startup IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
    /* wait 1ms */
    ksleep(1);

    if (smp_check_ap_flag()) {
        goto success;
    } else {
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

    tss_t *tss = &cpu_tss[0];

    #ifdef __X86_64__
        tss->sp = (uint64_t)cpu_stack_top;
    #endif
    #ifdef __I386__
        tss->sp = (uint32_t)cpu_stack_top;
        tss->ss = 0x08;
    #endif

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
