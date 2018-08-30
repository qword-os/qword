#include <ahci.h>
#include <ahci/hba.h>
#include <ahci/fis.h>
#include <pci.h>
#include <klib.h>

void init_ahci(void) {
    pci_device_t *device = kalloc(sizeof(pci_device_t));
    
    uint8_t class_mass_storage = 0x01;
    uint8_t subclass_serial_ata = 0x06;
    int ret = pci_get_device(device, class_mass_storage, subclass_serial_ata);
    if (ret == -1) kprint(KPRN_DBG, "Failed to find AHCI controller.");
}
