#ifndef __PCI_H__
#define __PCI_H__

#include <stdint.h>
#include <stddef.h>

#include <lai/core.h>

struct pci_device_t {
    int64_t parent;

    uint8_t bus;
    uint8_t func;
    uint8_t device;
    uint16_t device_id;
    uint16_t vendor_id;
    uint8_t rev_id;
    uint8_t subclass;
    uint8_t device_class;
    uint8_t prog_if;
    int multifunction;
    uint8_t irq_pin;

    lai_nsnode_t *acpi_node;

    int has_prt;
    lai_variable_t acpi_prt;

    uint32_t gsi;
};

struct pci_bar_t {
    uintptr_t base;
    size_t size;

    int is_mmio;
    int is_prefetchable;
};

uint32_t pci_read_device_byte(struct pci_device_t *device, uint32_t offset);
void pci_write_device_byte(struct pci_device_t *device, uint32_t offset, uint32_t value);
uint32_t pci_read_device_word(struct pci_device_t *device, uint32_t offset);
void pci_write_device_word(struct pci_device_t *device, uint32_t offset, uint32_t value);
uint32_t pci_read_device_dword(struct pci_device_t *device, uint32_t offset);
void pci_write_device_dword(struct pci_device_t *device, uint32_t offset, uint32_t value);

int pci_read_bar(struct pci_device_t *device, int bar, struct pci_bar_t *out);
void pci_enable_busmastering(struct pci_device_t *device);

struct pci_device_t *pci_get_device(uint8_t class, uint8_t subclass, uint8_t prog_if, size_t index);
struct pci_device_t *pci_get_device_by_vendor(uint16_t vendor, uint16_t id, size_t index);

void init_pci(void);

#endif
