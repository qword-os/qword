#ifndef __PCI_H__
#define __PCI_H__

#include <stdint.h>
#include <stddef.h>

struct pci_device_t {
    long parent;

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
};

uint32_t pci_read_bar0(struct pci_device_t *);
void pci_enable_busmastering(struct pci_device_t *);
void pci_check_function(uint8_t, uint8_t, uint8_t, long);
uint32_t pci_read_config(uint8_t, uint8_t, uint8_t, uint8_t);
uint32_t pci_read_device_byte(struct pci_device_t *device, uint32_t offset);
void pci_write_device_byte(struct pci_device_t *device, uint32_t offset, uint32_t value);
uint32_t pci_read_device_word(struct pci_device_t *device, uint32_t offset);
void pci_write_device_word(struct pci_device_t *device, uint32_t offset, uint32_t value);
uint32_t pci_read_device_dword(struct pci_device_t *device, uint32_t offset);
void pci_write_device_dword(struct pci_device_t *device, uint32_t offset, uint32_t value);
void pci_set_device_flag(struct pci_device_t *, uint32_t, uint32_t, int);
int pci_get_device(struct pci_device_t *, uint8_t, uint8_t, uint8_t);
int pci_get_device_by_vendor(struct pci_device_t *, uint16_t, uint16_t);
void pci_init_bus(uint8_t, long);
void init_pci(void);

#endif
