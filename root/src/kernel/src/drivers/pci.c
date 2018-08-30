#include <cio.h>
#include <stdint.h>
#include <pci.h>
#include <klib.h>

pci_device_t *pci_devices;
size_t device_count;

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus) << 16 | ((uint32_t)slot) << 11
       | ((uint32_t)func) << 8 | (uint32_t)(offset & 0xfc);

    port_out_d(0xcf8, address);
    return port_in_d(0xcfc);
}

/* Probe address space for information about a device */
void pci_probe(pci_device_t *device, uint8_t bus, uint8_t slot, uint8_t func) {
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
    if (config_c & 0x800000)
        device->multifunction = 1;
    else
        device->multifunction = 0;
    for (size_t i = 0; i < 6; i++) {
        device->bars[i] = 0;
    }
    
    device->available = 1;

    return;
}

uint32_t pci_get_device_address(pci_device_t *device, uint32_t offset) {
    return (1 << 31) | (((uint32_t)device->bus) << 16) | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | (((uint32_t)offset) & 0xfc);
}

uint32_t pci_read_device(pci_device_t *device, uint32_t offset) {
    uint32_t address = pci_get_device_address(device, offset);
    port_out_d(0xcf8, address);

    return port_in_d(0xcfc);
}

void pci_write_device(pci_device_t *device, uint32_t offset, uint32_t value) {
    uint32_t address = pci_get_device_address(device, offset);
    port_out_d(0xcf8, address);
    port_out_d(0xcfc, value);

    return;
}

void pci_set_device_flag(pci_device_t *device, uint32_t offset, uint32_t flag, int toggle) {
    uint32_t value = pci_read_device(device, offset);

    if (toggle)
        value |= flag;
    else
        value &= (0xffffffff - flag);
    pci_write_device(device, offset, value);
    
    return;
}

void pci_load_bars(pci_device_t *device) {
    for (size_t i = 0; i < 6; i++) {
        /* Bars exist at offset spacings of 4 bytes */
        uint32_t bar = pci_read_device(device, (i * 4) + 0x10);
        if (bar > 0) {
            device->bars[i] = bar;
            pci_write_device(device, i * 4 + 0x10, 0xffffffff);
            uint32_t size = (0xffffffff - (pci_read_device(device, i * 4 + 0x10) & 0xfffffff0)) + 1;
            pci_write_device(device, i * 4 + 0x10, bar);
            if (size > 0)
                device->bars[i] = size;
                return;
        }
    }
}

uint32_t pci_get_bar(pci_device_t *device, size_t index) {
    return device->bars[index];
}

int pci_get_device(pci_device_t *device, uint8_t class, uint8_t subclass) {
    for (size_t i = 0; i < device_count; i++) {
        if ((pci_devices[i].device_class == class) && (pci_devices[i].subclass == subclass)) {
            *device = pci_devices[i];
            return 0;
        }
    }

    return -1;
}

void pci_init_device(uint8_t bus, uint8_t dev) {
    for (uint8_t func = 0; func < MAX_FUNCTION; func++) {
search:
       for (size_t i = 0; i < device_count; i++) {
            if (pci_devices[i].available) {
                pci_device_t device = {0};
                pci_probe(&device, bus, dev, func);
                if (device.available) {
                    pci_load_bars(&device);
                    pci_devices[i] = device;
                    return;
                }
            }
       }

       device_count++;
       pci_devices = krealloc(pci_devices, (device_count) * sizeof(pci_device_t));
       goto search;
    }

    return;
}

void pci_init_bus(uint8_t bus) {
    for (uint8_t dev = 0; dev < MAX_DEVICE; dev++) {
        pci_init_device(bus, dev);
        return;
    }

    return;
}

void init_pci(void) {
    pci_devices = kalloc(8192 * sizeof(pci_device_t));
    device_count = 8192;

    for (size_t i = 0; i < device_count; i++) {
        pci_devices[i].available = 1;
    }

    for (uint8_t bus = 0; bus < MAX_BUS; bus++) {
        pci_init_bus(bus);
        return;
    }

    return;
}
