#include <lib/cio.h>
#include <stdint.h>
#include <sys/pci.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <lib/dynarray.h>

#define MAX_FUNCTION 8
#define MAX_DEVICE 32
#define MAX_BUS 256

dynarray_new(struct pci_device_t, pci_devices);
size_t available_devices;

uint32_t pci_read_device_byte(struct pci_device_t *device, uint32_t offset) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xffff);
    port_out_d(0xcf8, address);
    return port_in_d(0xcfc);
}

void pci_write_device_byte(struct pci_device_t *device, uint32_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xffff);
    port_out_d(0xcf8, address);
    port_out_d(0xcfc, value);
}

uint32_t pci_read_device_word(struct pci_device_t *device, uint32_t offset) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xfffe);
    port_out_d(0xcf8, address);
    return port_in_d(0xcfc);
}

void pci_write_device_word(struct pci_device_t *device, uint32_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xfffe);
    port_out_d(0xcf8, address);
    port_out_d(0xcfc, value);
}

uint32_t pci_read_device_dword(struct pci_device_t *device, uint32_t offset) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xfffc);
    port_out_d(0xcf8, address);
    return port_in_d(0xcfc);
}

void pci_write_device_dword(struct pci_device_t *device, uint32_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (offset & 0xfffc);
    port_out_d(0xcf8, address);
    port_out_d(0xcfc, value);
}

int pci_read_bar(struct pci_device_t *device, int bar, struct pci_bar_t *out) {
    if (bar > 5)
        return -1;

    size_t reg_index = 0x10 + bar * 4;
    uint64_t bar_low = pci_read_device_dword(device, reg_index), bar_size_low;
    uint64_t bar_high = 0, bar_size_high = 0;

    if (!bar_low)
        return -1;

    uintptr_t base;
    size_t size;

    int is_mmio = !(bar_low & 1);
    int is_prefetchable = is_mmio && bar_low & (1 << 3);
    int is_64bit = is_mmio && ((bar_low >> 1) & 0b11) == 0b10;

    if (is_64bit)
        bar_high = pci_read_device_dword(device, reg_index + 4);

    base = ((bar_high << 32) | bar_low) & ~(is_mmio ? (0b1111) : (0b11));

    pci_write_device_dword(device, reg_index, 0xFFFFFFFF);
    bar_size_low = pci_read_device_dword(device, reg_index);
    pci_write_device_dword(device, reg_index, bar_low);

    if (is_64bit) {
        pci_write_device_dword(device, reg_index + 4, 0xFFFFFFFF);
        bar_size_high = pci_read_device_dword(device, reg_index + 4);
        pci_write_device_dword(device, reg_index + 4, bar_high);
    }

    size = ((bar_size_high << 32) | bar_size_low) & ~(is_mmio ? (0b1111) : (0b11));
    size = ~size + 1;

    if (out) {
        *out = (struct pci_bar_t){base, size, is_mmio, is_prefetchable};
    }

    return 0;
}

void pci_enable_busmastering(struct pci_device_t *device) {
    if (!(pci_read_device_dword(device, 0x4) & (1 << 2))) {
        pci_write_device_dword(device, 0x4, pci_read_device_dword(device, 0x4) | (1 << 2));
    }
}

struct pci_device_t *pci_get_device(uint8_t class, uint8_t subclass, uint8_t prog_if) {
    return dynarray_search(struct pci_device_t, pci_devices, elem->device_class == class
                            && elem->subclass == subclass && elem->prog_if == prog_if);
}

struct pci_device_t *pci_get_device_by_vendor(uint16_t vendor, uint16_t id) {
    return dynarray_search(struct pci_device_t, pci_devices,
            elem->vendor_id == vendor && elem->device_id == id);
}

static void pci_check_bus(uint8_t, int64_t);

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func, int64_t parent) {
    struct pci_device_t device = {0};
    device.bus = bus;
    device.func = func;
    device.device = slot;

    uint32_t config_0 = pci_read_device_dword(&device, 0);

    if (config_0 == 0xffffffff) {
        return;
    }

    uint32_t config_8 = pci_read_device_dword(&device, 0x8);
    uint32_t config_c = pci_read_device_dword(&device, 0xc);

    device.parent = parent;
    device.device_id = (uint16_t)(config_0 >> 16);
    device.vendor_id = (uint16_t)config_0;
    device.rev_id = (uint8_t)config_8;
    device.subclass = (uint8_t)(config_8 >> 16);
    device.device_class = (uint8_t)(config_8 >> 24);
    device.prog_if = (uint8_t)(config_8 >> 8);

    if (config_c & 0x800000)
        device.multifunction = 1;
    else
        device.multifunction = 0;

    size_t id = dynarray_add(struct pci_device_t, pci_devices, &device);
    available_devices++;

    if ((config_c & 0x7f) == 1) {
        // pci to pci bridge
        uint32_t config_18 = pci_read_device_dword(&device, 0x18);
        pci_check_bus((config_18 >> 8) & 0xFF, id);
    }
}

static void pci_check_bus(uint8_t bus, int64_t parent) {
    for (size_t dev = 0; dev < MAX_DEVICE; dev++) {
        for (size_t func = 0; func < MAX_FUNCTION; func++) {
            pci_check_function(bus, dev, func, parent);
        }
    }
}

static void pci_init_root_bus(void) {
    struct pci_device_t device = {0};
    uint32_t config_c = pci_read_device_dword(&device, 0xc);
    uint32_t config_0;

    if (!(config_c & 0x800000)) {
        pci_check_bus(0, -1);
    } else {
        for (size_t func = 0; func < 8; func++) {
            device.func = func;
            config_0 = pci_read_device_dword(&device, 0);
            if (config_0 == 0xffffffff)
                continue;

            pci_check_bus(func, -1);
        }
    }
}

void init_pci(void) {
    pci_init_root_bus();

    kprint(KPRN_INFO, "pci: Full recursive device scan done, %u devices found", available_devices);
    kprint(KPRN_INFO, "pci: List of found devices:");

    for (size_t i = 0; i < available_devices; i++) {
        struct pci_device_t *dev = dynarray_getelem(struct pci_device_t, pci_devices, i);

        kprint(KPRN_INFO, "pci:\t%2x:%2x.%1x - %4x:%4x", dev->bus, dev->device, dev->func, dev->vendor_id, dev->device_id);

        if (dev->parent != -1) {
            struct pci_device_t *parent_dev = dynarray_getelem(struct pci_device_t, pci_devices, dev->parent);
            kprint(KPRN_INFO, "pci:\t\tparent: %2x:%2x.%1x - %4x:%4x", 
                    parent_dev->bus, parent_dev->device, parent_dev->func, parent_dev->vendor_id, parent_dev->device_id);
            dynarray_unref(pci_devices, dev->parent);
        } else {
            kprint(KPRN_INFO, "pci:\t\ton root bus");
        }

        dynarray_unref(pci_devices, i);
    }

}
