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

// TODO: Fix acpi_find_sdt to support multiple tables with the same signature
void *laihost_scan(char *signature, size_t index) {
    // The DSDT is a special case, as it must be located using the pointer found in the FADT
    if (!strncmp(signature, "DSDT", 4)) {
        // Scan for the FADT
        acpi_fadt_t *fadt = (struct fadt_t *)acpi_find_sdt("FACP");
        void *dsdt = (char *)(size_t)fadt + 36 + MEM_PHYS_OFFSET;
        return dsdt;
    } else {
        return acpi_find_sdt(signature);
    }
}
