#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <acpi.h>

#define ACPI_TABLES_MAX 256

int acpi_available = 0;
int madt_available = 0;

static int use_xsdt = 0;

rsdp_t *rsdp;
rsdt_t *rsdt;
xsdt_t *xsdt;
madt_t *madt;

madt_local_apic_t **madt_local_apics;
size_t madt_local_apic_ptr = 0;

madt_io_apic_t **madt_io_apics;
size_t madt_io_apic_ptr = 0;

madt_iso_t **madt_isos;
size_t madt_iso_ptr = 0;

madt_nmi_t **madt_nmis;
size_t madt_nmi_ptr = 0;

/* This function should look for all the ACPI tables and index them for
   later use */
void init_acpi(void) {
    kprint(KPRN_INFO, "acpi: Initialising...");

    /* look for the "RSD PTR " signature from 0x80000 to 0xa0000 and from
       0xf0000 to 0x100000 */
    for (size_t i = 0x80000; i < 0x100000; i += 16) {
        if (i == 0xa0000) {
            /* skip video mem and mapped hardware */
            i = 0xe0000 - 16;
            continue;
        }
        if (!kstrncmp((char *)i, "RSD PTR ", 8)) {
            kprint(KPRN_INFO, "acpi: Found RSDP at %x", i);
            rsdp = (rsdp_t *)i;
            goto rsdp_found;
        }
    }
    acpi_available = 0;
    kprint(KPRN_INFO, "acpi: Non-ACPI compliant system");
    return;
rsdp_found:

    acpi_available = 1;
    kprint(KPRN_INFO, "acpi: ACPI available");

    kprint(KPRN_INFO, "acpi: Revision: %u", (uint32_t)rsdp->rev);

    if (rsdp->rev >= 2 && rsdp->xsdt_addr) {
        /* FIXME: use XSDT whenever available */
        /*kprint(KPRN_INFO, "acpi: Found XSDT at %x", rsdp->xsdt_addr);
        xsdt = (xsdt_t *)rsdp->xsdt_addr;*/
        use_xsdt = 1;
        kprint(KPRN_INFO, "acpi: Found RSDT at %x", rsdp->rsdt_addr);
        rsdt = (rsdt_t *)(size_t)rsdp->rsdt_addr;
    } else {
        kprint(KPRN_INFO, "acpi: Found RSDT at %x", rsdp->rsdt_addr);
        rsdt = (rsdt_t *)(size_t)rsdp->rsdt_addr;
    }

    /****** MADT ******/

    /* search for MADT table */
    for (size_t i = 0; i < rsdt->sdt.length; i++) {
        madt = (madt_t *)(size_t)rsdt->sdt_ptr[i];
        if (!kstrncmp(madt->sdt.signature, "APIC", 4)) {
            kprint(KPRN_INFO, "acpi: Found MADT at %x", (size_t)madt);
            goto madt_found;
        }
    }
    madt_available = 0;
    kprint(KPRN_INFO, "acpi: MADT not found");
    goto madt_not_found;
madt_found:

    madt_available = 1;

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
                kprint(KPRN_INFO, "acpi: Found local APIC #%u", madt_local_apic_ptr);
                madt_local_apics[madt_local_apic_ptr++] = (madt_local_apic_t *)madt_ptr;
                break;
            case 1:
                /* I/O APIC */
                kprint(KPRN_INFO, "acpi: Found I/O APIC #%u", madt_io_apic_ptr);
                madt_io_apics[madt_io_apic_ptr++] = (madt_io_apic_t *)madt_ptr;
                break;
            case 2:
                /* interrupt source override */
                kprint(KPRN_INFO, "acpi: Found ISO #%u", madt_iso_ptr);
                madt_isos[madt_iso_ptr++] = (madt_iso_t *)madt_ptr;
                break;
            case 4:
                /* NMI */
                kprint(KPRN_INFO, "acpi: Found NMI #%u", madt_nmi_ptr);
                madt_nmis[madt_nmi_ptr++] = (madt_nmi_t *)madt_ptr;
                break;
            default:
                break;
        }
    }
madt_not_found:

    return;

}
