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

uint32_t pci_read_bar0(struct pci_device_t *device) {
    return pci_read_device_dword(device, 0x10);
}

void pci_enable_busmastering(struct pci_device_t *device) {
    if (!(pci_read_device_dword(device, 0x4) & (1 << 2))) {
        pci_write_device_dword(device, 0x4, pci_read_device_dword(device, 0x4) | (1 << 2));
    }
}

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus) << 16 | ((uint32_t)slot) << 11
       | ((uint32_t)func) << 8 | (uint32_t)(offset & 0xfc);

    port_out_d(0xcf8, address);
    return port_in_d(0xcfc);
}

void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func, long parent) {
    struct pci_device_t device = {0};

    uint32_t config_0 = pci_read_config(bus, slot, func, 0);

    if (config_0 == 0xffffffff) {
        return;
    }

    uint32_t config_8 = pci_read_config(bus, slot, func, 0x8);
    uint32_t config_c = pci_read_config(bus, slot, func, 0xc);

    device.parent = parent;
    device.bus = bus;
    device.func = func;
    device.device = slot;
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
        uint32_t config_18 = pci_read_config(bus, slot, func, 0x18);
        pci_init_bus((config_18 >> 8) & 0xFF, id);
    }
}

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

void pci_set_device_flag(struct pci_device_t *device, uint32_t offset, uint32_t flag, int toggle) {
    uint32_t value = pci_read_device_dword(device, offset);

    if (toggle)
        value |= flag;
    else
        value &= (0xffffffff - flag);
    pci_write_device_dword(device, offset, value);
}

int pci_get_device(struct pci_device_t *device, uint8_t class, uint8_t subclass, uint8_t prog_if) {
    struct pci_device_t *dev;
    dev = dynarray_search(struct pci_device_t, pci_devices, elem->device_class == class
                            && elem->subclass == subclass && elem->prog_if == prog_if);

    if (dev) {
        *device = *dev;
        return 0;
    }

    return -1;
}

int pci_get_device_by_vendor(struct pci_device_t *device, uint16_t vendor, uint16_t id) {
    struct pci_device_t *dev;
    dev = dynarray_search(struct pci_device_t, pci_devices,
            elem->vendor_id == vendor && elem->device_id == id);

    if (dev) {
        *device = *dev;
        return 0;
    }

    return -1;
}

void pci_init_bus(uint8_t bus, long parent) {
    for (size_t dev = 0; dev < MAX_DEVICE; dev++) {
        for (size_t func = 0; func < MAX_FUNCTION; func++) {
            pci_check_function(bus, dev, func, parent);
        }
    }
}

void pci_init_root_bus(void) {
    uint32_t config_c = pci_read_config(0, 0, 0, 0xc);
    uint32_t config_0;

    if (!(config_c & 0x800000)) {
        pci_init_bus(0, -1);
    } else {
        for (size_t func = 0; func < 8; func++) {
            config_0 = pci_read_config(0, 0, func, 0);
            if (config_0 == 0xffffffff)
                continue;

            pci_init_bus(func, -1);
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
