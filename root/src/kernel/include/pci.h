#ifndef __PCI_H__
#define __PCI_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t bus;
    uint8_t func;
    uint8_t device;
    uint16_t device_id;
    uint16_t vendor_id;
    uint8_t rev_id;
    uint8_t subclass;
    uint8_t device_class;
    int multifunction;
    uint32_t bars[6];
}pci_device_t;

void pci_probe(pci_device_t *, uint8_t, uint8_t, uint8_t);
uint32_t pci_read_config(uint8_t, uint8_t, uint8_t, uint8_t);
uint32_t pci_read_device(pci_device_t *, uint32_t);
void pci_write_device(pci_device_t *, uint32_t, uint32_t);
uint32_t pci_get_device_address(pci_device_t *, uint32_t);
void pci_set_device_flag(pci_device_t *, uint32_t, uint32_t, int);
void pci_load_bars(pci_device_t *device);
uint32_t pci_get_bar(pci_device_t *, size_t);

#endif
