#ifndef __MADT_H__
#define __MADT_H__

#include <stdint.h>
#include <stddef.h>
#include <acpi.h>

typedef struct {
    sdt_t sdt;
    uint32_t local_controller_addr;
    uint32_t flags;
    uint8_t madt_entries_begin;
} __attribute__((packed)) madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_header_t;

typedef struct {
    madt_header_t madt_header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) madt_local_apic_t;

typedef struct {
    madt_header_t madt_header;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t gsib;
} __attribute__((packed)) madt_io_apic_t;

typedef struct {
    madt_header_t madt_header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) madt_iso_t;

typedef struct {
    madt_header_t madt_header;
    uint8_t processor;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed)) madt_nmi_t;

extern int madt_available;

extern madt_t *madt;

extern madt_local_apic_t **madt_local_apics;
extern size_t madt_local_apic_ptr;

extern madt_io_apic_t **madt_io_apics;
extern size_t madt_io_apic_ptr;

extern madt_iso_t **madt_isos;
extern size_t madt_iso_ptr;

extern madt_nmi_t **madt_nmis;
extern size_t madt_nmi_ptr;

void init_madt(void);

#endif
