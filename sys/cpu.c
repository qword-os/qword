#include <sys/cpu.h>
#include <cpuid.h>
#include <lib/klib.h>

unsigned int cpu_simd_region_size;

void (*cpu_save_simd)(uint8_t *);
void (*cpu_restore_simd)(uint8_t *);

#define XSAVE_BIT (1 << 26)
#define AVX_BIT (1 << 28)
#define AVX512_BIT (1 << 16)

void init_cpu_features() {
    uint32_t a, b, c, d;
    __get_cpuid(1, &a, &b, &c, &d);

    if ((c & XSAVE_BIT)) {
        uint64_t cr4 = 0;
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
    if((uintptr_t)region & 0x3F)
        panic("Xsave region is not aligned correctly!", 0, 0, NULL);

    asm volatile("xsave %0" : "=m"(*(uint8_t(*)[cpu_simd_region_size])region) : "a"(0xFFFFFFFF), "d"(0xFFFFFFFF) : "memory");
}

void xrstor(uint8_t *region) {
    if((uintptr_t)region & 0x3F)
        panic("Xsave region is not aligned correctly!", 0, 0, NULL);

    asm volatile("xrstor %0" : : "m"(*(uint8_t(*)[cpu_simd_region_size])region), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF) : "memory");
}

void fxsave(uint8_t *region) {
    asm volatile ("fxsave %0" : "=m" (*(uint8_t(*)[512])(region)) : : "memory");
}

void fxrstor(uint8_t *region) {
    asm volatile ("fxrstor %0" : : "m" (*(uint8_t(*)[512])(region)) : "memory");
}
