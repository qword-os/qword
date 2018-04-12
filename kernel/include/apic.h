#ifndef __APIC_H__
#define __APIC_H__

#include <stdint.h>

void check_apic(void);

extern int should_use_apic;

#define APIC_CPUID_BIT 1 << 9

#endif
