#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <acpi.h>
#include <acpi/madt.h>

int acpi_available = 0;

static int use_xsdt = 0;

rsdp_t *rsdp;
rsdt_t *rsdt;
xsdt_t *xsdt;

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
        use_xsdt = 1;
        kprint(KPRN_INFO, "acpi: Found XSDT at %x", (uint32_t)rsdp->xsdt_addr);
        xsdt = (xsdt_t *)(size_t)rsdp->xsdt_addr;
    } else {
        kprint(KPRN_INFO, "acpi: Found RSDT at %x", (uint32_t)rsdp->rsdt_addr);
        rsdt = (rsdt_t *)(size_t)rsdp->rsdt_addr;
    }

    /* Call table inits */
    init_madt();

    return;
}

/* Find SDT by signature */
void *acpi_find_sdt(const char *signature) {
    sdt_t *ptr;

    if (use_xsdt) {
        for (size_t i = 0; i < xsdt->sdt.length; i++) {
            ptr = (sdt_t *)(size_t)xsdt->sdt_ptr[i];
            if (!kstrncmp(ptr->signature, signature, 4)) {
                kprint(KPRN_INFO, "acpi: Found \"%s\" at %x", signature, (uint32_t)(size_t)ptr);
                return (void *)ptr;
            }
        }
    } else {
        for (size_t i = 0; i < rsdt->sdt.length; i++) {
            ptr = (sdt_t *)(size_t)rsdt->sdt_ptr[i];
            if (!kstrncmp(ptr->signature, signature, 4)) {
                kprint(KPRN_INFO, "acpi: Found \"%s\" at %x", signature, (uint32_t)(size_t)ptr);
                return (void *)ptr;
            }
        }
    }

    kprint(KPRN_INFO, "acpi: \"%s\" not found", signature);
    return (void *)0;
}
