#include <lib/klib.h>
#include <lib/alloc.h>
#include <sys/panic.h>
#include <acpi/lai/host.h>
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
