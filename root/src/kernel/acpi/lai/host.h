
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018-2019 by Omar Muhamed
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

struct lai_object_t;
typedef struct lai_object_t lai_object_t;

#define LAI_DEBUG_LOG 1
#define LAI_WARN_LOG 2

// OS-specific functions.
void *laihost_malloc(size_t);
void *laihost_realloc(void *, size_t);
void laihost_free(void *);

__attribute__((weak)) void laihost_log(int, const char *, va_list);
__attribute__((weak, noreturn)) void laihost_panic(const char *, va_list);

__attribute__((weak)) void *laihost_scan(char *, size_t);
__attribute__((weak)) void *laihost_map(size_t, size_t);
__attribute__((weak)) void laihost_outb(uint16_t, uint8_t);
__attribute__((weak)) void laihost_outw(uint16_t, uint16_t);
__attribute__((weak)) void laihost_outd(uint16_t, uint32_t);
__attribute__((weak)) uint8_t laihost_inb(uint16_t);
__attribute__((weak)) uint16_t laihost_inw(uint16_t);
__attribute__((weak)) uint32_t laihost_ind(uint16_t);
__attribute__((weak)) void laihost_pci_write(uint8_t, uint8_t, uint8_t, uint16_t, uint32_t);
__attribute__((weak)) uint32_t laihost_pci_read(uint8_t, uint8_t, uint8_t, uint16_t);
__attribute__((weak)) void laihost_sleep(uint64_t);

__attribute__((weak)) void laihost_handle_amldebug(lai_object_t *);

