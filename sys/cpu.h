#ifndef __SYS__CPU_H__
#define __SYS__CPU_H__

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

extern void (*cpu_save_simd)(void *);
extern void (*cpu_restore_simd)(void *);

void init_cpu_features();

static inline int cpuid(uint32_t leaf, uint32_t subleaf,
                        uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t cpuid_max;
    asm volatile ("cpuid"
                  : "=a" (cpuid_max)
                  : "a" (leaf & 0x80000000));
    if (leaf > cpuid_max)
        return 0;
    asm volatile ("cpuid"
                  : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                  : "a" (leaf), "c" (subleaf));
    return 1;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx, eax;
    asm volatile ("rdmsr"
                  : "=a" (eax), "=d" (edx)
                  : "c" (msr));
    return ((uint64_t)edx << 32) | eax;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("wrmsr"
                  :
                  : "a" (eax), "d" (edx), "c" (msr));
}

static inline void wrxcr(uint32_t i, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("xsetbv"
                  :
                  : "a" (eax), "d" (edx), "c" (i));
}

static inline void xsave(void *region) {
    asm volatile ("xsave [%0]"
                  :
                  : "r" (region), "a" (0xFFFFFFFF), "d" (0xFFFFFFFF)
                  : "memory");
}

static inline void xrstor(void *region) {
    asm volatile ("xrstor [%0]"
                  :
                  : "r" (region), "a" (0xFFFFFFFF), "d" (0xFFFFFFFF)
                  : "memory");
}

static inline void fxsave(void *region) {
    asm volatile ("fxsave [%0]"
                  :
                  : "r" (region)
                  : "memory");
}

static inline void fxrstor(void *region) {
    asm volatile ("fxrstor [%0]"
                  :
                  : "r" (region)
                  : "memory");
}

#endif
