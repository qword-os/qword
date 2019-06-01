#include <lib/klib.h>
#include <lib/alloc.h>
#include <lib/cio.h>
#include <sys/panic.h>
#include <lai/host.h>
#include <acpispec/tables.h>
#include <acpi/acpi.h>
#include <stdarg.h>
#include <misc/pci.h>

void laihost_log(int level, const char *str) {
    switch (level) {
        case LAI_DEBUG_LOG:
            kprint(KPRN_DBG, str);
            break;
        case LAI_WARN_LOG:
            kprint(KPRN_WARN, str);
            break;
        default:
            kprint(KPRN_WARN, str);
            break;
    }

    return;
}

void laihost_panic(const char *str) {
    panic(str, 0, 0, NULL);
}

void *laihost_malloc(size_t size) {
    return kalloc(size);
}

void *laihost_realloc(void *p, size_t size) {
    return krealloc(p, size);
}

void laihost_free(void *p) {
    return kfree(p);
}

void *laihost_scan(char *signature, size_t index) {
    // The DSDT is a special case, as it must be located using the pointer found in the FADT
    if (!strncmp(signature, "DSDT", 4)) {
        // Scan for the FADT
        acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_find_sdt("FACP", 0);
        void *dsdt = (char *)(size_t)fadt->dsdt + MEM_PHYS_OFFSET;
        kprint(KPRN_INFO, "acpi: Address of DSDT is %X", dsdt);
        return dsdt;
    } else {
        return acpi_find_sdt(signature, index);
    }
}

uint8_t laihost_inb(uint16_t port) {
    return port_in_b(port);
}

void laihost_outb(uint16_t port, uint8_t value) {
    port_out_b(port, value);
}

uint16_t laihost_inw(uint16_t port) {
    return port_in_w(port);
}

void laihost_outw(uint16_t port, uint16_t value) {
    port_out_w(port, value);
}

uint32_t laihost_ind(uint16_t port) {
    return port_in_d(port);
}

void laihost_outd(uint16_t port, uint32_t value) {
    port_out_d(port, value);
}

void laihost_sleep(uint64_t duration) {
    ksleep(duration);
}

void laihost_pci_write(uint8_t bus, uint8_t function, uint8_t device, uint16_t offset, uint32_t data) {
    struct pci_device_t *dev = kalloc(sizeof(struct pci_device_t));
    dev->bus = bus;
    dev->func = function;
    dev->device = device;

    pci_write_device(dev, (uint32_t)offset, data);
    kfree(dev);
}

uint32_t laihost_pci_read(uint8_t bus, uint8_t function, uint8_t device, uint16_t offset) {
    struct pci_device_t *dev = kalloc(sizeof(struct pci_device_t));
    dev->bus = bus;
    dev->func = function;
    dev->device = device;

    return pci_read_device(dev, (uint32_t)offset);
}

void *laihost_map(size_t phys_addr, size_t count) {
    // all physical memory is mapped into the higher half, so we can just return
    // the physical address + the offset into the higher half.
    size_t virt_addr = phys_addr + MEM_PHYS_OFFSET;
    return (void *)virt_addr;
}
