#include <stdint.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <acpi/madt.h>
#include <panic.h>
#include <smp.h>
#include <time.h>

#define MAX_CPUS 128
#define CPU_STACK_SIZE (8192+8192)

int smp_cpu_count = 1;

static size_t cpu_stack_top = 0xeffff0;

static void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "smp: Started up AP #%u", smp_get_cpu_number());
    kprint(KPRN_INFO, "smp: AP #%u kernel stack top: %x", smp_get_cpu_number(), smp_get_cpu_kernel_stack());

    /* halt and catch fire */
    for (;;) { asm volatile ("cli; hlt"); }
}

extern void *kernel_pagemap;

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    if (cpu_number == MAX_CPUS) {
        panic("smp: CPU limit exceeded", smp_cpu_count, 0);
    }

    /* create CPU local struct */
    cpu_local_t *cpu_local = kalloc(sizeof(cpu_local_t));
    if (!cpu_local)
        panic("", 0, 0);

    cpu_local->cpu_number = cpu_number;
    cpu_local->kernel_stack = cpu_stack_top;

    void *trampoline = smp_prepare_trampoline(ap_kernel_entry, &kernel_pagemap, (void *)cpu_stack_top, cpu_local);

    cpu_stack_top -= CPU_STACK_SIZE;

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
        return 0;
    } else {
        /* Send the Startup IPI again */
        lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
        lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
        /* wait 1s */
        ksleep(1000);
        if (smp_check_ap_flag())
            return 0;
        else
            return -1;
    }
}

static void init_cpu0(void) {
    /* create CPU 0 local struct */
    cpu_local_t *cpu_local = kalloc(sizeof(cpu_local_t));
    if (!cpu_local)
        panic("", 0, 0);

    cpu_local->cpu_number = 0;
    cpu_local->kernel_stack = cpu_stack_top;
    cpu_stack_top -= CPU_STACK_SIZE;

    smp_init_cpu0_local(cpu_local);

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
