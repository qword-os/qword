#include <lib/cio.h>
#include <stdint.h>
#include <sys/pci.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <lib/dynarray.h>
#include <sys/panic.h>
#include <lai/helpers/pci.h>

#define MAX_FUNCTION 8
#define MAX_DEVICE 32
#define MAX_BUS 256

dynarray_new(struct pci_device_t, pci_devices);
size_t available_devices;

#define BYTE  0
#define WORD  1
#define DWORD 2

static void get_address(int s, struct pci_device_t *device, uint32_t offset) {
    switch (s) {
        case BYTE:
            offset &= 0xffff;
        case WORD:
            offset &= 0xfffe;
        case DWORD:
            offset &= 0xfffc;
    }
    port_out_d(0xcf8, (1 << 31) | (((uint32_t)device->bus) << 16)
        | (((uint32_t)device->device) << 11)
        | (((uint32_t)device->func) << 8) | offset);
}

uint8_t pci_read_device_byte(struct pci_device_t *device, uint32_t offset) {
    get_address(BYTE, device, offset);
    return port_in_b(0xcfc + (offset % 4));
}

void pci_write_device_byte(struct pci_device_t *device, uint32_t offset, uint8_t value) {
    get_address(BYTE, device, offset);
    port_out_b(0xcfc + (offset % 4), value);
}

uint16_t pci_read_device_word(struct pci_device_t *device, uint32_t offset) {
    get_address(WORD, device, offset);
    return port_in_w(0xcfc + (offset % 4));
}

void pci_write_device_word(struct pci_device_t *device, uint32_t offset, uint16_t value) {
    get_address(WORD, device, offset);
    port_out_w(0xcfc + (offset % 4), value);
}

uint32_t pci_read_device_dword(struct pci_device_t *device, uint32_t offset) {
    get_address(DWORD, device, offset);
    return port_in_d(0xcfc + (offset % 4));
}

void pci_write_device_dword(struct pci_device_t *device, uint32_t offset, uint32_t value) {
    get_address(DWORD, device, offset);
    port_out_d(0xcfc + (offset % 4), value);
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

struct pci_device_t *pci_get_device(uint8_t class, uint8_t subclass, uint8_t prog_if, size_t index) {
    size_t i;
    struct pci_device_t *ret = dynarray_search(struct pci_device_t, pci_devices, &i, elem->device_class == class
                            && elem->subclass == subclass && elem->prog_if == prog_if, index);
    dynarray_unref(pci_devices, i);
    return ret;
}

struct pci_device_t *pci_get_device_by_vendor(uint16_t vendor, uint16_t id, size_t index) {
    size_t i;
    struct pci_device_t *ret = dynarray_search(struct pci_device_t, pci_devices, &i,
            elem->vendor_id == vendor && elem->device_id == id, index);
    dynarray_unref(pci_devices, i);
    return ret;
}

#define PCI_PNP_ID  "PNP0A03"
#define PCIE_PNP_ID "PNP0A08"

static lai_nsnode_t *pci_determine_root_bus_node(uint8_t bus, lai_state_t *state) {
    LAI_CLEANUP_VAR lai_variable_t pci_pnp_id = LAI_VAR_INITIALIZER;
    LAI_CLEANUP_VAR lai_variable_t pcie_pnp_id = LAI_VAR_INITIALIZER;
    lai_eisaid(&pci_pnp_id, PCI_PNP_ID);
    lai_eisaid(&pcie_pnp_id, PCIE_PNP_ID);

    lai_nsnode_t *sb_handle = lai_resolve_path(NULL, "\\_SB_");
    panic_unless(sb_handle);

    struct lai_ns_child_iterator iter = LAI_NS_CHILD_ITERATOR_INITIALIZER(sb_handle);
    lai_nsnode_t *node;

    while ((node = lai_ns_child_iterate(&iter))) {
        if (lai_check_device_pnp_id(node, &pci_pnp_id, state) &&
            lai_check_device_pnp_id(node, &pcie_pnp_id, state)) {
                continue;
        }

        // this is a root bus
        LAI_CLEANUP_VAR lai_variable_t bus_number = LAI_VAR_INITIALIZER;
        uint64_t bbn_result = 0;
        lai_nsnode_t *bbn_handle = lai_resolve_path(node, "_BBN");
        if (bbn_handle) {
            if (lai_eval(&bus_number, bbn_handle, state)) {
                continue;
            }
            lai_obj_get_integer(&bus_number, &bbn_result);
        }

        if (bbn_result == bus)
            return node;
    }

    return NULL;
}

static void pci_determine_acpi_node_for(struct pci_device_t *device, lai_state_t *state) {
    if (device->acpi_node)
        return;

    lai_nsnode_t *node = NULL;

    if (device->parent != -1) {
        struct pci_device_t *parent = dynarray_getelem(struct pci_device_t, pci_devices, device->parent);
        if (!parent)
            return;

        if(!parent->acpi_node)
            pci_determine_acpi_node_for(parent, state);

        node = parent->acpi_node;
    } else {
        node = pci_determine_root_bus_node(device->bus, state);
    }

    if (!node)
        return;

    device->acpi_node = lai_pci_find_device(node, device->device, device->func, state);
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
    uint32_t config_3c = pci_read_device_dword(&device, 0x3c);

    device.parent = parent;
    device.device_id = (uint16_t)(config_0 >> 16);
    device.vendor_id = (uint16_t)config_0;
    device.rev_id = (uint8_t)config_8;
    device.subclass = (uint8_t)(config_8 >> 16);
    device.device_class = (uint8_t)(config_8 >> 24);
    device.prog_if = (uint8_t)(config_8 >> 8);
    device.irq_pin = (uint8_t)(config_3c >> 8);

    if (config_c & 0x800000)
        device.multifunction = 1;
    else
        device.multifunction = 0;

    size_t id = dynarray_add(struct pci_device_t, pci_devices, &device);
    available_devices++;

    if (device.device_class == 0x06 && device.subclass == 0x04) {
        // pci to pci bridge
        struct pci_device_t *device = dynarray_getelem(struct pci_device_t, pci_devices, id);

        LAI_CLEANUP_STATE lai_state_t state;
        lai_init_state(&state);

        // attempt to find PRT for this bridge
        pci_determine_acpi_node_for(device, &state);

        if (device->acpi_node) {
            lai_nsnode_t *prt_handle = lai_resolve_path(device->acpi_node, "_PRT");

            if (prt_handle) {
                device->has_prt = !lai_eval(&device->acpi_prt, prt_handle, &state);
            }
        }

        dynarray_unref(pci_devices, id);

        // find devices attached to this bridge
        uint32_t config_18 = pci_read_device_dword(device, 0x18);
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

static void pci_route_interrupts(void) {
    LAI_CLEANUP_STATE lai_state_t state;
    lai_init_state(&state);

    for (size_t i = 0; i < available_devices; i++) {
        struct pci_device_t *dev = dynarray_getelem(struct pci_device_t, pci_devices, i);
        dynarray_unref(pci_devices, i);

        if (!dev->irq_pin)
            continue;

        uint8_t irq_pin = dev->irq_pin;
        struct pci_device_t *tmp = dev;
        LAI_CLEANUP_VAR lai_variable_t root_prt = LAI_VAR_INITIALIZER;
        lai_variable_t *prt = NULL;

        while(1) {
            if (tmp->parent != -1) {
                struct pci_device_t *parent = dynarray_getelem(struct pci_device_t, pci_devices, tmp->parent);
                dynarray_unref(pci_devices, tmp->parent);

                if (!parent->has_prt) {
                    irq_pin = (((irq_pin - 1) + (tmp->device % 4)) % 4) + 1;
                } else {
                    prt = &parent->acpi_prt;
                    break;
                }
                tmp = parent;
            } else {
                lai_nsnode_t *node = pci_determine_root_bus_node(tmp->bus, &state);

                lai_nsnode_t *prt_handle = lai_resolve_path(node, "_PRT");
                if (prt_handle) {
                    if (lai_eval(&root_prt, prt_handle, &state))
                        break;
                }

                prt = &root_prt;
                break;
            }
        }

        if (!prt) {
            kprint(KPRN_WARN, "failed to get prt for device");
            continue;
        }

        struct lai_prt_iterator iter = LAI_PRT_ITERATOR_INITIALIZER(prt);
        lai_api_error_t err;

        while (!(err = lai_pci_parse_prt(&iter))) {
            if (iter.slot == tmp->device &&
                    (iter.function == tmp->func || iter.function == -1) &&
                    iter.pin == (irq_pin - 1)) {
                // TODO: care about flags for the IRQ

                dev->gsi = iter.gsi;
                dev->gsi_flags |= (!!iter.active_low) << 2;
                dev->gsi_flags |= (!!iter.level_triggered) << 8;

                kprint(KPRN_INFO, "pci: device %2x:%2x.%1x routed to gsi %u",
                        dev->bus, dev->device, dev->func, iter.gsi);
                break;
            }
        }
    }
}

int pci_register_msi(struct pci_device_t *device, uint8_t vector) {
    uint8_t off = 0;

    uint32_t config_4 = pci_read_device_dword(device, 0x4);
    uint8_t  config_34 = pci_read_device_byte(device, 0x34);

    if((config_4 >> 16) & (1 << 4)) {
        uint8_t cap_off = config_34;

        while(cap_off) {
            uint8_t cap_id = pci_read_device_byte(device, cap_off);
            uint8_t cap_next = pci_read_device_byte(device, cap_off + 1);

            switch(cap_id) {
                case 0x05: {
                    kprint(KPRN_INFO, "pci: device has msi support");
                    off = cap_off;
                    break;
                }
            }
            cap_off = cap_next;
        }
    }

    if(off == 0) {
        kprint(KPRN_INFO, "pci: device does not support msi");
        return 0;
    }

    uint16_t msi_opts = pci_read_device_word(device, off + MSI_OPT);
    if(msi_opts & MSI_64BIT_SUPPORTED) {
        union msi_data_t data = {0};
        union msi_address_t addr = {0};
        addr.raw = pci_read_device_word(device, off + MSI_ADDR_LOW);
        data.raw = pci_read_device_word(device, off + MSI_DATA_64);
        data.vector = vector;
        //Fixed delivery mode
        data.delivery_mode = 0;
        addr.base_address = 0xFEE;
        addr.destination_id = cpu_locals[current_cpu].lapic_id;
        pci_write_device_dword(device, off + MSI_ADDR_LOW, addr.raw);
        pci_write_device_dword(device, off + MSI_DATA_64, data.raw);
    } else {
        union msi_data_t data = {0};
        union msi_address_t addr = {0};
        addr.raw = pci_read_device_word(device, off + MSI_ADDR_LOW);
        data.raw = pci_read_device_word(device, off + MSI_DATA_32);
        data.vector = vector;
        //Fixed delivery mode
        data.delivery_mode = 0;
        addr.base_address = 0xFEE;
        addr.destination_id = cpu_locals[current_cpu].lapic_id;
        pci_write_device_dword(device, off + MSI_ADDR_LOW, addr.raw);
        pci_write_device_dword(device, off + MSI_DATA_32, data.raw);
    }
    msi_opts |= 1;
    pci_write_device_word(device, off + MSI_OPT, msi_opts);
    return 1;
}

void init_pci(void) {
    pci_init_root_bus();

    kprint(KPRN_INFO, "pci: Full recursive device scan done, %u devices found", available_devices);
    kprint(KPRN_INFO, "pci: List of found devices:");

    for (size_t i = 0; i < available_devices; i++) {
        struct pci_device_t *dev = dynarray_getelem(struct pci_device_t, pci_devices, i);

        kprint(KPRN_INFO, "pci:\t%2x:%2x.%1x - %4x:%4x", dev->bus, dev->device, dev->func, dev->vendor_id, dev->device_id);

        dynarray_unref(pci_devices, i);
    }

    pci_route_interrupts();
}
