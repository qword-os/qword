#include <lib/klib.h>
#include <lib/alloc.h>
#include <sys/panic.h>
#include <lai/host.h>
#include <acpispec/tables.h>
#include <acpi/acpi.h>
#include <stdarg.h>

void laihost_log(int level, const char *fmt, va_list args) {
    kvprint(level, fmt, args);
}

void laihost_panic(const char *fmt, va_list args) {
    panic(fmt, 0, 0, NULL);
}

void *laihost_malloc(size_t size) {
    return kalloc(size);
}

void *laihost_realloc(void *p, size_t size) {
    return krealloc(p, size);
}

void laihost_free(void *p) {
    return kfree(p);
}

void *laihost_scan(char *signature, size_t index) {
    // The DSDT is a special case, as it must be located using the pointer found in the FACP
    if (!strncmp(signature, "DSDT", 4)) {
        // Scan for the FACP
        acpi_facp_t *facp = (acpi_facp_t *)acpi_find_sdt("FACP", 0);
        void *dsdt = (char *)(size_t)facp->dsdt + 36 + MEM_PHYS_OFFSET;
        kprint(KPRN_INFO, "acpi: Address of DSDT is %X", dsdt);
        return dsdt;
    } else {
        return acpi_find_sdt(signature, index);
    }
}
