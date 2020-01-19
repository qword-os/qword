#ifndef __CPU_H__
#define __CPU_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/types.h>

#define MAX_CPUS 128

#define current_cpu ({ \
    size_t cpu_number; \
    asm volatile ("mov %0, qword ptr gs:[0]" \
                    : "=r" (cpu_number) \
                    : \
                    : "memory", "cc"); \
    (int)cpu_number; \
})

struct cpu_local_t {
    /* DO NOT MOVE THESE MEMBERS FROM THESE LOCATIONS */
    /* DO NOT CHANGE THEIR TYPES */
    size_t cpu_number;
    size_t kernel_stack;
    size_t thread_kstack;
    size_t thread_ustack;
    size_t thread_errno;
    size_t ipi_abort_received;
    /* Feel free to move every other member, and use any type as you see fit */
    tid_t current_task;
    pid_t current_process;
    tid_t current_thread;
    int64_t last_schedule_time;
    uint8_t lapic_id;
    int ipi_abortexec_received;
    int ipi_resched_received;
};

extern struct cpu_local_t cpu_locals[MAX_CPUS];

extern unsigned int cpu_simd_region_size;

extern void (*cpu_save_simd)(uint8_t *);
extern void (*cpu_restore_simd)(uint8_t *);

void init_cpu_features();

void wrxcr(uint32_t index, uint64_t value);

uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t value);

void xsave(uint8_t *region);
void xrstor(uint8_t *region);

void fxsave(uint8_t *region);
void fxrstor(uint8_t *region);

#endif
