#include <stdint.h>
#include <stddef.h>
#include <devices/storage/nvme/nvme.h>
#include <misc/pci.h>
#include "nvme_private.h"
#include <lib/klib.h>
#include <devices/dev.h>

size_t nvme_base;

static uint32_t nvme_rread(size_t reg) {
    return *((volatile uint32_t *)(nvme_base + reg));
}

static void nvme_wread(size_t reg, uint32_t data) {
    *((volatile uint32_t *)(nvme_base + reg)) = data;
}

void init_dev_nvme(void) {
    struct pci_device_t device = {0};
    int ret = pci_get_device(&device, NVME_CLASS, NVME_SUBCLASS, NVME_PROG_IF);
    if (ret == -1) {
        kprint(KPRN_INFO, "nvme: Failed to locate NVME controller");
        return;
    }

    kprint(KPRN_INFO, "nvme: Found NVME controller");

    size_t bar0 = pci_read_device(&device, 0x10);
    size_t bar1 = pci_read_device(&device, 0x14);

    nvme_base = (size_t)((uint64_t)bar0 << 32 | bar1);

    size_t cap = nvme_rread(REG_CAP);
    // calculate doorbell stride
    size_t dstrd = cap >> 32;

    kprint(KPRN_DBG, "%x", dstrd);
}
