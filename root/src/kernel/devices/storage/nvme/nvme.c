#include <stdint.h>
#include <stddef.h>
#include <devices/storage/nvme/nvme.h>
#include <misc/pci.h>
#include "nvme_private.h"
#include <lib/klib.h>

size_t nvme_base;

static uint32_t nvme_rread(size_t reg) {
    return *((volatile uint32_t *)(nvme_base + reg));
}

static void nvme_wread(size_t reg, uint32_t data) {
    *((volatile uint32_t *)(nvme_base + reg)) = data;
    return;
}

// FIXME: this function exists only as a placeholder for now
void init_nvme(void) {
    uint8_t class_mass_storage = 0x01;
    uint8_t subclass_nvm_controller = 0x08;

    struct pci_device_t device = {0};
    int ret = pci_get_device(&device, class_mass_storage, subclass_nvm_controller);
    if (ret == -1) {
        kprint(KPRN_INFO, "nvme: Failed to locate NVM controller");
        return;
    }

    kprint(KPRN_INFO, "nvme: Found NVM controller");

    // Now find a device with an prog if matching that of NVME
    // TODO: build this functionality into PCI interface
    size_t prog_if = (pci_read_device(&device, 0x8)) >> 8; // possibly broken
    if (prog_if != 0x02) {
        kprint(KPRN_INFO, "nvme: Controller is non-nvme, exiting...");
        return;
    }

    size_t bar0 = pci_read_device(&device, 0x10);
    size_t bar1 = pci_read_device(&device, 0x14);

    nvme_base = (size_t)((uint64_t)bar0 << 32 | bar1);

    size_t cap = nvme_rread(REG_CAP);
    // calculate doorbell stride
    size_t dstrd = cap >> 32;

    kprint(KPRN_DBG, "%x", dstrd);

    return;
}
