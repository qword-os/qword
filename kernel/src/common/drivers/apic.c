#include <stdint.h>
#include <stddef.h>
#include <apic.h>
#include <klib.h>
#include <cpuid.h>
#include <acpi/madt.h>

/* TODO Add inter-processor interrupts */

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

uint32_t lapic_read(uint32_t reg) {
    size_t lapic_base = (size_t)madt->local_controller_addr;
    return *((volatile uint32_t *)(lapic_base + reg));
}

void lapic_write(uint32_t reg, uint32_t data) {
    size_t lapic_base = (size_t)madt->local_controller_addr;
    *((volatile uint32_t *)(lapic_base + reg)) = data;
}

void lapic_set_nmi(uint8_t vec, uint16_t flags, uint8_t lint) {
    uint32_t nmi = 800 | vec;

    if (flags & 2) {
        nmi |= (1 << 13);
    }

    if (flags & 8) {
        nmi |= (1 << 15);
    }
    
    if (lint == 1) {
        lapic_write(0x360, nmi);
    } else if (lint == 0) {
        lapic_write(0x350, nmi);
    }
}

void lapic_install_nmis(void) {
    for (size_t i = 0; i < madt_nmi_ptr; i++)
        /* Reserve vectors 0x90 .. lengthof(madt_nmi_ptr) for NMIs. */
        lapic_set_nmi(0x90 + i, madt_nmis[i]->flags, madt_nmis[i]->lint);
}

void lapic_enable(void) {
    lapic_write(0xf0, lapic_read(0xf0) | 0x1ff);
}

void lapic_eoi(void) {
    lapic_write(0xb0, 0);
}

void lapic_send_ipi(uint8_t vector, uint8_t target_id) {
    lapic_write(0x300, ((uint32_t)target_id << 24));
    lapic_write(0x310, 0x4000 | vector);
}

/* Read from the `io_apic_num`'th I/O APIC as described by the MADT */
uint32_t io_apic_read(size_t io_apic_num, uint32_t reg) {
    volatile uint32_t *base = (volatile uint32_t *)(size_t)madt_io_apics[io_apic_num]->addr;
    *base = reg;
    return *(base + 4);
}

/* Write to the `io_apic_num`'th I/O APIC as described by the MADT */
void io_apic_write(size_t io_apic_num, uint32_t reg, uint32_t data) {
    volatile uint32_t *base = (volatile uint32_t *)(size_t)madt_io_apics[io_apic_num]->addr;
    *base = reg;
    *(base + 4) = data;
    return;
}

/* Return the index of the I/O APIC that handles this redirect */
size_t io_apic_from_redirect(uint32_t gsi) {
    for (size_t i = 0; i < madt_io_apic_ptr; i++) {
        if (madt_io_apics[i]->gsib <= gsi && madt_io_apics[i]->gsib + io_apic_get_max_redirect(i) > gsi)
            return i;
    }

    return -1;
}

/* Get the maximum number of redirects this I/O APIC can handle */
uint32_t io_apic_get_max_redirect(size_t io_apic_num) {
    return (io_apic_read(io_apic_num, 1) & 0xff0000) >> 16;
}

void io_apic_set_redirect(uint8_t irq, uint32_t gsi, uint16_t flags, uint8_t apic) {
    size_t io_apic = io_apic_from_redirect(gsi);
    
    /* Map APIC irqs to vectors beginning after exceptions */
    uint64_t redirect = irq + 0x20;

    if (flags & 2) {
        redirect |= (1 << 13);
    }

    if (flags & 8) {
        redirect |= (1 << 15);
    }

    /* Set target APIC ID */
    redirect |= ((uint64_t)apic) << 56;

    uint32_t ioredtbl = (gsi - madt_io_apics[io_apic]->gsib) * 2 + 16;

    io_apic_write(io_apic, ioredtbl + 0, (uint32_t)redirect);
    io_apic_write(io_apic, ioredtbl + 1, (uint32_t)(redirect >> 32));
}

void io_apic_set_mask(int irq, int status) {
    if (status) {
        /* install IRQ ISO */
        for (size_t i = 0; i < madt_iso_ptr; i++) {
            if (madt_isos[i]->irq_source == irq) {
                io_apic_set_redirect(madt_isos[i]->irq_source, madt_isos[i]->gsi,
                                madt_isos[i]->flags, madt_local_apics[0]->apic_id);
                return;
            }
        }
        /* not found in the ISOs, redirect normally */
        io_apic_set_redirect(irq, irq, 0, madt_local_apics[0]->apic_id);
    } else {
        /* TODO: code to mask APIC IRQs */
    }

    return;
}

void init_apic(void) {
    kprint(KPRN_INFO, "apic: Installing non-maskable interrupts...");
    lapic_install_nmis();
    kprint(KPRN_INFO, "apic: Enabling local APIC...");
    lapic_enable();
    kprint(KPRN_INFO, "apic: Done! APIC initialised.");
}
