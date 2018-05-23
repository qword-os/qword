#include <stdint.h>
#include <stddef.h>
#include <acpi.h>
#include <acpi/madt.h>
#include <klib.h>

int madt_available = 0;
madt_t *madt;

madt_local_apic_t **madt_local_apics;
size_t madt_local_apic_ptr = 0;

madt_io_apic_t **madt_io_apics;
size_t madt_io_apic_ptr = 0;

madt_iso_t **madt_isos;
size_t madt_iso_ptr = 0;

madt_nmi_t **madt_nmis;
size_t madt_nmi_ptr = 0;

void init_madt(void) {
    /* search for MADT table */
    if ((madt = acpi_find_sdt("APIC"))) {
        madt_available = 1;
    } else {
        madt_available = 0;
        return;
    }

    madt_local_apics = kalloc(ACPI_TABLES_MAX);
    madt_io_apics = kalloc(ACPI_TABLES_MAX);
    madt_isos = kalloc(ACPI_TABLES_MAX);
    madt_nmis = kalloc(ACPI_TABLES_MAX);

    /* parse the MADT entries */
    for (uint8_t *madt_ptr = (uint8_t *)(&madt->madt_entries_begin);
        (size_t)madt_ptr < (size_t)madt + madt->sdt.length;
        madt_ptr += *(madt_ptr + 1)) {
        switch (*(madt_ptr)) {
            case 0:
                /* processor local APIC */
                kprint(KPRN_INFO, "acpi/madt: Found local APIC #%u", madt_local_apic_ptr);
                madt_local_apics[madt_local_apic_ptr++] = (madt_local_apic_t *)madt_ptr;
                break;
            case 1:
                /* I/O APIC */
                kprint(KPRN_INFO, "acpi/madt: Found I/O APIC #%u", madt_io_apic_ptr);
                madt_io_apics[madt_io_apic_ptr++] = (madt_io_apic_t *)madt_ptr;
                break;
            case 2:
                /* interrupt source override */
                kprint(KPRN_INFO, "acpi/madt: Found ISO #%u", madt_iso_ptr);
                madt_isos[madt_iso_ptr++] = (madt_iso_t *)madt_ptr;
                break;
            case 4:
                /* NMI */
                kprint(KPRN_INFO, "acpi/madt: Found NMI #%u", madt_nmi_ptr);
                madt_nmis[madt_nmi_ptr++] = (madt_nmi_t *)madt_ptr;
                break;
            default:
                break;
        }
    }

    return;
}
