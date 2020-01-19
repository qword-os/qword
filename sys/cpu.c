#include <sys/cpu.h>
#include <cpuid.h>
#include <lib/klib.h>
#include <sys/panic.h>

unsigned int cpu_simd_region_size;

void (*cpu_save_simd)(uint8_t *);
void (*cpu_restore_simd)(uint8_t *);

uint64_t rdmsr(uint32_t msr) {
    uint32_t edx, eax;
    asm volatile ("rdmsr"
                  : "=a" (eax), "=d" (edx)
                  : "c" (msr));
    return ((uint64_t)edx << 32) | eax;
}

void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("wrmsr"
                  :
                  : "a" (eax), "d" (edx), "c" (msr));
}

#define XSAVE_BIT (1 << 26)
#define AVX_BIT (1 << 28)
#define AVX512_BIT (1 << 16)

void syscall_entry(void);

void init_cpu_features(void) {
    // First enable SSE/SSE2 as it is baseline for x86_64
    uint64_t cr0 = 0;
    asm volatile ("mov %0, %%cr0" : "=r" (cr0));
    cr0 &= ~(1 << 2);
    cr0 |=  (1 << 1);
    asm volatile ("mov %%cr0, %0" : : "r" (cr0));

    uint64_t cr4 = 0;
    asm volatile ("mov %0, %%cr4" : "=r" (cr4));
    cr4 |= (3 << 9);
    asm volatile ("mov %%cr4, %0" : : "r" (cr4));

    // Initialise the PAT
    uint64_t pat_msr = rdmsr(0x277);
    pat_msr &= 0xffffffff;
    // write-protect / write-combining
    pat_msr |= (uint64_t)0x0105 << 32;
    wrmsr(0x277, pat_msr);

    // Enable syscall in EFER
    uint64_t efer = rdmsr(0xc0000080);
    efer |= 1;
    wrmsr(0xc0000080, efer);

    // Set up syscall
    wrmsr(0xc0000081, 0x0013000800000000);
    // Syscall entry address
    wrmsr(0xc0000082, (uint64_t)syscall_entry);
    // Flags mask
    wrmsr(0xc0000084, ~0x002);

    uint32_t a, b, c, d;
    __get_cpuid(1, &a, &b, &c, &d);

    if ((c & XSAVE_BIT)) {
        asm volatile ("mov %0, %%cr4" : "=r" (cr4) : : "memory");
        cr4 |= (1 << 18); // Enable XSAVE and x{get, set}bv
        asm volatile ("mov %%cr4, %0" : : "r" (cr4) : "memory");

        uint64_t xcr0 = 0;
        xcr0 |= (1 << 0); // Save x87 state with xsave
        xcr0 |= (1 << 1); // Save SSE state with xsave

        if ((c & AVX_BIT))
            xcr0 |= (1 << 2); // Enable AVX and save AVX state with xsave

        if (__get_cpuid_count(7, 0, &a, &b, &c, &d)) {
            if((b & AVX512_BIT)) {
                xcr0 |= (1 << 5); // Enable AVX-512
                xcr0 |= (1 << 6); // Enable management of ZMM{0 -> 15}
                xcr0 |= (1 << 7); // Enable management of ZMM{16 -> 31}
            }
        }
        wrxcr(0, xcr0);

        if (__get_cpuid_count(0xD, 0, &a, &b, &c, &d)) {
            cpu_simd_region_size = c;
        } else {
            panic("Enabled xsave but cpuid leaf doesn't exist", 0, 0, NULL);
        }

        cpu_save_simd = xsave;
        cpu_restore_simd = xrstor;
    } else {
        cpu_simd_region_size = 512; // Legacy size for fxsave
        cpu_save_simd = fxsave;
        cpu_restore_simd = fxrstor;
    }
}

void wrxcr(uint32_t index, uint64_t value) {
    uint32_t low = value;
    uint32_t high = value >> 32;
    asm volatile ("xsetbv" : : "c" (index), "a" (low), "d" (high) : "memory");
}

void xsave(uint8_t *region) {
    asm volatile("xsave %0" : "=m"(*(uint8_t(*)[cpu_simd_region_size])region) : "a"(0xFFFFFFFF), "d"(0xFFFFFFFF) : "memory");
}

void xrstor(uint8_t *region) {
    asm volatile("xrstor %0" : : "m"(*(uint8_t(*)[cpu_simd_region_size])region), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF) : "memory");
}

void fxsave(uint8_t *region) {
    asm volatile ("fxsave %0" : "=m" (*(uint8_t(*)[512])(region)) : : "memory");
}

void fxrstor(uint8_t *region) {
    asm volatile ("fxrstor %0" : : "m" (*(uint8_t(*)[512])(region)) : "memory");
}
