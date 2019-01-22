#ifndef __MADT_H__
#define __MADT_H__

#include <stdint.h>
#include <stddef.h>
#include <acpi/acpi.h>

struct madt_t {
    struct sdt_t sdt;
    uint32_t local_controller_addr;
    uint32_t flags;
    uint8_t madt_entries_begin;
} __attribute__((packed));

struct madt_header_t {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_local_apic_t {
    struct madt_header_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_io_apic_t {
    struct madt_header_t header;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t gsib;
} __attribute__((packed));

struct madt_iso_t {
    struct madt_header_t header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

struct madt_nmi_t {
    struct madt_header_t header;
    uint8_t processor;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed));

extern int madt_available;

extern struct madt_t *madt;

extern struct madt_local_apic_t **madt_local_apics;
extern size_t madt_local_apic_i;

extern struct madt_io_apic_t **madt_io_apics;
extern size_t madt_io_apic_i;

extern struct madt_iso_t **madt_isos;
extern size_t madt_iso_i;

extern struct madt_nmi_t **madt_nmis;
extern size_t madt_nmi_i;

void init_madt(void);

#endif
