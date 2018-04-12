#include <apic.h>
#include <klib.h>
#include <cpuid.h>

int should_use_apic = 0;

void check_apic(void) {
	uint32_t eax, ebx, ecx, edx = 0;
	__get_cpuid(1, &eax, &ebx, &ecx, &edx);
    
    kprint(KPRN_INFO, "Checking APIC support...");

    /* Check if the apic bit is set */
    if ((edx & APIC_CPUID_BIT)) {
        kprint(KPRN_INFO, "CPU supports APIC!");
        should_use_apic = 1;
    }
}
