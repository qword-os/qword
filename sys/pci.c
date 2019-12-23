#include <lib/cio.h>
#include <stdint.h>
#include <sys/pci.h>
#include <lib/klib.h>
#include <lib/alloc.h>

#define MAX_FUNCTION 8
#define MAX_DEVICE 32
#define MAX_BUS 256

struct pci_device_t *pci_devices;
size_t device_count;
size_t available_count;

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

/* Probe address space for information about a device */
void pci_probe(struct pci_device_t *device, uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t config_0 = pci_read_config(bus, slot, func, 0);

    if (config_0 == 0xffffffff) {
        device->available = 0;
        return;
    }

    uint32_t config_8 = pci_read_config(bus, slot, func, 0x8);
    uint32_t config_c = pci_read_config(bus, slot, func, 0xc);

    device->available = 1;
    device->bus = bus;
    device->func = func;
    device->device = slot;
    device->device_id = (uint16_t)(config_0 >> 16);
    device->vendor_id = (uint16_t)config_0;
    device->rev_id = (uint8_t)config_8;
    device->subclass = (uint8_t)(config_8 >> 16);
    device->device_class = (uint8_t)(config_8 >> 24);
    device->prog_if = (uint8_t)(config_8 >> 8);
    if (config_c & 0x800000)
        device->multifunction = 1;
    else
        device->multifunction = 0;
    device->available = 1;
    available_count++;
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
    for (size_t i = 0; i < device_count; i++) {
        if ((pci_devices[i].device_class == class)
            && (pci_devices[i].subclass == subclass)
            && (pci_devices[i].prog_if == prog_if)) {
            *device = pci_devices[i];
            return 0;
        }
    }

    return -1;
}

int pci_get_device_by_vendor(struct pci_device_t *device, uint16_t vendor, uint16_t id) {
    for (size_t i = 0; i < device_count; i++) {
        if ((pci_devices[i].vendor_id == vendor) && (pci_devices[i].device_id == id)) {
            *device = pci_devices[i];
            return 0;
        }
    }

    return -1;
}

void pci_init_device(uint8_t bus, uint8_t dev) {
    for (size_t func = 0; func < MAX_FUNCTION; func++) {
        pci_find_function(bus, dev, func);
    }
}

void pci_find_function(uint8_t bus, uint8_t dev, uint8_t func) {
    struct pci_device_t device = {0};
    pci_probe(&device, bus, dev, func);
    if (device.available) {
        for (size_t i = 0; i < device_count; i++) {
            if (!pci_devices[i].available) {
                pci_devices[i] = device;
                return;
            }
        }

        pci_devices = krealloc(pci_devices, (device_count + 1) * sizeof(struct pci_device_t));
        pci_devices[device_count] = device;
        pci_devices++;

        return;
    }
}

void pci_init_bus(uint8_t bus) {
    for (size_t dev = 0; dev < MAX_DEVICE; dev++) {
        pci_init_device(bus, dev);
    }
}

void init_pci(void) {
    pci_devices = kalloc(8192 * sizeof(struct pci_device_t));
    device_count = 8192;
    available_count = 0;

    for (size_t i = 0; i < device_count; i++) {
        pci_devices[i].available = 1;
    }

    for (size_t bus = 0; bus < MAX_BUS; bus++) {
        pci_init_bus(bus);
    }

    kprint(KPRN_INFO, "pci: Full recursive device scan done, %u devices found", available_count);
}
