#ifndef __ACPI_H__
#define __ACPI_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t rsdt_addr;
    /* ver 2.0 only */
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t rev;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__((packed)) acpi_sdt_t;

typedef struct {
    acpi_sdt_t sdt;
    uint32_t sdt_ptr[];
} __attribute__((packed)) rsdt_t;

typedef struct {
    acpi_sdt_t sdt;
    uint64_t sdt_ptr[];
} __attribute__((packed)) xsdt_t;

typedef struct {
    acpi_sdt_t sdt;
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

extern int acpi_available;
extern int madt_available;

extern rsdp_t *rsdp;
extern rsdt_t *rsdt;
extern xsdt_t *xsdt;
extern madt_t *madt;

extern madt_local_apic_t **madt_local_apics;
extern size_t madt_local_apic_ptr;

extern madt_io_apic_t **madt_io_apics;
extern size_t madt_io_apic_ptr;

extern madt_iso_t **madt_isos;
extern size_t madt_iso_ptr;

extern madt_nmi_t **madt_nmis;
extern size_t madt_nmi_ptr;

void init_acpi(void);

#endif
