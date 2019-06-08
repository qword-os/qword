#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <lib/cmdline.h>
#include <acpi/acpi.h>
#include <acpi/madt.h>
#include <lai/core.h>
#include <acpispec/tables.h>
#include <mm/mm.h>
#include <sys/idt.h>

int acpi_available = 0;

static int use_xsdt = 0;

struct rsdp_t *rsdp;
struct rsdt_t *rsdt;
struct xsdt_t *xsdt;

void sci_handler(int, struct regs_t *);

/* This function should look for all the ACPI tables and index them for
   later use */
void init_acpi(void) {
    kprint(KPRN_INFO, "acpi: Initialising...");

    /* look for the "RSD PTR " signature from 0x80000 to 0xa0000 and from
       0xf0000 to 0x100000 */
    for (size_t i = 0x80000 + MEM_PHYS_OFFSET; i < 0x100000 + MEM_PHYS_OFFSET; i += 16) {
        if (i == 0xa0000 + MEM_PHYS_OFFSET) {
            /* skip video mem and mapped hardware */
            i = 0xe0000 - 16 + MEM_PHYS_OFFSET;
            continue;
        }
        if (!strncmp((char *)i, "RSD PTR ", 8)) {
            kprint(KPRN_INFO, "acpi: Found RSDP at %X", i);
            rsdp = (struct rsdp_t *)i;
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
        kprint(KPRN_INFO, "acpi: Found XSDT at %X", ((size_t)rsdp->xsdt_addr + MEM_PHYS_OFFSET));
        xsdt = (struct xsdt_t *)((size_t)rsdp->xsdt_addr + MEM_PHYS_OFFSET);
    } else {
        kprint(KPRN_INFO, "acpi: Found RSDT at %X", ((size_t)rsdp->rsdt_addr + MEM_PHYS_OFFSET));
        rsdt = (struct rsdt_t *)((size_t)rsdp->rsdt_addr + MEM_PHYS_OFFSET);
    }

    /* Call table inits */
    init_madt();

    char *cmdline_val = cmdline_get_value("acpi");
    if (!cmdline_val || !strcmp(cmdline_val, "enabled"))
        lai_create_namespace();

    acpi_fadt_t *fadt = acpi_find_sdt("FACP", 0);
    if (fadt) {
        uint16_t irq = fadt->sci_irq;
        void (*handlers[1])(int, struct regs_t *) = {sci_handler};
        io_apic_set_mask(irq, irq, 1);

        int ret = register_isr((size_t)irq + 0x20, handlers, 1, 1, 0x8e);
    }

    return;
}

void sci_handler(int num, struct regs_t *regs) {
    // Use lai to determine whether this irq was ACPI-related.
    uint16_t event = lai_get_sci_event();

    const char *name = "?";
    if (event & ACPI_POWER_BUTTON) name = "The power button was pressed.";
    if (event & ACPI_SLEEP_BUTTON) name = "The sleep button was pressed";
    if (event & ACPI_WAKE) name = "The system woke up from sleep";

    kprint(KPRN_DBG, "acpi: SCI event occured: %X: %s", event, name);

    return;
}

/* Find SDT by signature */
void *acpi_find_sdt(const char *signature, int index) {
    struct sdt_t *ptr;
    int cnt = 0;

    if (use_xsdt) {
        for (size_t i = 0; i < (xsdt->sdt.length - sizeof(struct sdt_t)) / 8; i++) {
            ptr = (struct sdt_t *)((size_t)xsdt->sdt_ptr[i] + MEM_PHYS_OFFSET);
            if (!strncmp(ptr->signature, signature, 4)) {
                if (cnt++ == index) {
                    kprint(KPRN_INFO, "acpi: Found \"%s\" at %X", signature, (size_t)ptr);
                    return (void *)ptr;
                }
            }
        }
    } else {
        for (size_t i = 0; i < (rsdt->sdt.length - sizeof(struct sdt_t)) / 4; i++) {
            ptr = (struct sdt_t *)((size_t)rsdt->sdt_ptr[i] + MEM_PHYS_OFFSET);
            if (!strncmp(ptr->signature, signature, 4)) {
                if (cnt++ == index) {
                    kprint(KPRN_INFO, "acpi: Found \"%s\" at %X", signature, (size_t)ptr);
                    return (void *)ptr;
                }
            }
        }
    }

    kprint(KPRN_INFO, "acpi: \"%s\" not found", signature);
    return (void *)0;
}
