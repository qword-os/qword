#include <apic.h>
#include <klib.h>
#include <cpuid.h>

#define APIC_CPUID_BIT (1 << 9)

int apic_supported(void) {
    unsigned int eax, ebx, ecx, edx = 0;

    kprint(KPRN_INFO, "apic: Checking for support...");

    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    /* Check if the apic bit is set */
    if ((edx & APIC_CPUID_BIT)) {
        kprint(KPRN_INFO, "apic: Supported!");
        return 1;
    } else {
        kprint(KPRN_INFO, "apic: Unsupported!");
        return 0;
    }
}
