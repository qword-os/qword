#include <stdint.h>
#include <stddef.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <fs/devfs/devfs.h>
#include <misc/pci.h>
#include <mm/mm.h>
#include <lib/errno.h>
#include <lib/part.h>

#define DEVICE_COUNT 4
#define BYTES_PER_SECT 512

#define MAX_CACHED_SECTORS 8192

#define CACHE_NOT_READY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2

typedef struct {
    uint8_t *cache;
    uint64_t sector;
    int status;
} cached_sector_t;

struct prdt_t {
    uint32_t buffer_phys;
    uint16_t transfer_size;
    uint16_t mark_end;
} __attribute__((packed));

typedef struct {
    int exists;

    int master;

    uint16_t identify[256];

    uint16_t data_port;
    uint16_t error_port;
    uint16_t sector_count_port;
    uint16_t lba_low_port;
    uint16_t lba_mid_port;
    uint16_t lba_hi_port;
    uint16_t device_port;
    uint16_t command_port;
    uint16_t control_port;

    uint64_t sector_count;
    uint16_t bytes_per_sector;

    uint32_t bar4;
    uint32_t bmr_command;
    uint32_t bmr_status;
    uint32_t bmr_prdt;
    struct prdt_t *prdt;
    uint32_t prdt_phys;
    uint8_t *prdt_cache;

    cached_sector_t *cache;
} ide_device;

static const char *ide_basename = "ide";

static const uint16_t ide_ports[] = { 0x1f0, 0x1f0, 0x170, 0x170 };
static const int max_ports = 4;

static int ide_read(int drive, void *buf, uint64_t loc, size_t count);
static int ide_write(int drive, const void *buf, uint64_t loc, size_t count);
static ide_device init_ide_device(uint16_t port_base, int master,
        struct pci_device_t *pci);
static void ide_identify(ide_device* dev, struct pci_device_t *pci);
static int ide_read28(int disk, uint32_t sector, uint8_t *buffer);
static int ide_read48(int disk, uint64_t sector, uint8_t *buffer);
static int ide_write28(int disk, uint32_t sector, uint8_t *buffer);
static int ide_write48(int disk, uint64_t sector, uint8_t *buffer);
static int ide_flush(int disk);
static int ide_flush_ext(int disk);

static ide_device ide_devices[DEVICE_COUNT];

static lock_t ide_lock = new_lock;

static int find_sect(int drive, uint64_t sect) {
    for (size_t i = 0; i < MAX_CACHED_SECTORS; i++)
        if ((ide_devices[drive].cache[i].sector == sect)
            && (ide_devices[drive].cache[i].status))
            return i;

    return -1;

}

static int overwritten_slot = 0;

static int cache_sect(int drive, int sect) {
    int targ;
    int ret;

    /* Find empty sector */
    for (targ = 0; targ < MAX_CACHED_SECTORS; targ++)
        if (!ide_devices[drive].cache[targ].status) goto fnd;

    /* Slot not find, overwrite another */
    if (overwritten_slot == MAX_CACHED_SECTORS)
        overwritten_slot = 0;

    targ = overwritten_slot++;

    /* Flush device cache */
    if (ide_devices[drive].cache[targ].status == CACHE_DIRTY) {
        if (sect <= 0x0fffffff)
            ret = ide_write28(drive, (uint32_t)ide_devices[drive].cache[targ].sector, ide_devices[drive].cache[targ].cache);
        else
            ret = ide_write48(drive, ide_devices[drive].cache[targ].sector, ide_devices[drive].cache[targ].cache);

        if (ret == -1) return -1;
    }

    goto notfnd;

fnd:
    /* Allocate some cache for this device */
    ide_devices[drive].cache[targ].cache = kalloc(BYTES_PER_SECT);

notfnd:

    /* Load sector into cache */
    if (sect <= 0x0fffffff)
        ret = ide_read28(drive, (uint32_t)sect, ide_devices[drive].cache[targ].cache);
    else
        ret = ide_read48(drive, sect, ide_devices[drive].cache[targ].cache);

    if (ret == -1) return -1;

    ide_devices[drive].cache[targ].sector = sect;
    ide_devices[drive].cache[targ].status = CACHE_READY;

    return targ;
}

static int ide_read(int drive, void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&ide_lock);

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the sector */
        uint64_t sect = (loc + progress) / BYTES_PER_SECT;
        int slot = find_sect(drive, sect);
        if (slot == -1) {
            slot = cache_sect(drive, sect);
            if (slot == -1) {
                spinlock_release(&ide_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_SECT;
        if (chunk > BYTES_PER_SECT - offset)
            chunk = BYTES_PER_SECT - offset;

        memcpy(buf + progress, &ide_devices[drive].cache[slot].cache[offset], chunk);
        progress += chunk;
    }

    spinlock_release(&ide_lock);
    return (int)count;
}

static int ide_write(int drive, const void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&ide_lock);

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the sector */
        uint64_t sect = (loc + progress) / BYTES_PER_SECT;
        int slot = find_sect(drive, sect);
        if (slot == -1) {
            slot = cache_sect(drive, sect);
            if (slot == -1) {
                spinlock_release(&ide_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_SECT;
        if (chunk > BYTES_PER_SECT - offset)
            chunk = BYTES_PER_SECT - offset;

        memcpy(&ide_devices[drive].cache[slot].cache[offset], buf + progress, chunk);
        ide_devices[drive].cache[slot].status = CACHE_DIRTY;
        progress += chunk;
    }

    spinlock_release(&ide_lock);
    return (int)count;
}

static int ide_flush1(int device) {
    spinlock_acquire(&ide_lock);

    for (size_t i = 0; i < MAX_CACHED_SECTORS; i++) {
        if (ide_devices[device].cache[i].status == CACHE_DIRTY) {
            int ret;

            if (ide_devices[device].cache[i].sector <= 0x0fffffff)
                ret = ide_write28(device, (uint32_t)ide_devices[device].cache[i].sector, ide_devices[device].cache[i].cache);
            else
                ret = ide_write48(device, ide_devices[device].cache[i].sector, ide_devices[device].cache[i].cache);

            if (ret == -1) {
                spinlock_release(&ide_lock);
                return -1;
            }

            ide_devices[device].cache[i].status = CACHE_READY;
        }
    }

    spinlock_release(&ide_lock);
    return 0;
}

void init_dev_ide(void) {
    kprint(KPRN_INFO, "ide: Initialising ide device driver...");

    struct pci_device_t pci_device = {0};
    // TODO figure out correct prog if and define these values elsewhere
    int ret = pci_get_device(&pci_device, 0x1, 0x1, 0x85);
    if (ret) {
        kprint(KPRN_ERR, "ide: could not find pci device!");
        return;
    }

    int j = 0;
    int master = 1;
    for (int i = 0; i < DEVICE_COUNT; i++) {
        if (j >= max_ports) return;
        while (!(ide_devices[i] = init_ide_device(ide_ports[j], master,
                        &pci_device)).exists) {
            j++;
            if (j >= max_ports) return;
            if (j % 2) master = 0;
            else master = 1;
        }
        j++;
        if (j % 2) master = 0;
        else master = 1;
        struct device_t device = {0};
        device.calls = default_device_calls;
        char *dev_name = prefixed_itoa(ide_basename, i, 10);
        strcpy(device.name, dev_name);
        kprint(KPRN_INFO, "ide: Initialised /dev/%s", dev_name);
        kfree(dev_name);
        device.intern_fd = i;
        device.size = ide_devices[i].sector_count * 512;
        device.calls.read = ide_read;
        device.calls.write = ide_write;
        device.calls.flush = ide_flush1;
        device_add(&device);
        enum_partitions(dev_name, &device);
    }
}

static ide_device init_ide_device(uint16_t port_base, int master,
        struct pci_device_t *pci) {
    ide_device dev;

    dev.data_port = port_base;
    dev.error_port = port_base + 0x01;
    dev.sector_count_port = port_base + 0x02;
    dev.lba_low_port = port_base + 0x03;
    dev.lba_mid_port = port_base + 0x04;
    dev.lba_hi_port = port_base + 0x05;
    dev.device_port = port_base + 0x06;
    dev.command_port = port_base + 0x07;
    dev.control_port = port_base + 0x206;
    dev.exists = 0;
    dev.master = master;

    dev.bar4 = pci_read_device(pci, 0x20);
    if (dev.bar4 & 0x1)
        dev.bar4 &= 0xFFFFFFFC;
    dev.bmr_command = dev.bar4;
    dev.bmr_status = dev.bar4 + 0x2;
    dev.bmr_prdt = dev.bar4 + 0x4;

    dev.bytes_per_sector = 512;

    ide_identify(&dev, pci);

    return dev;
}

static void ide_identify(ide_device* dev, struct pci_device_t *pci) {
    uint32_t cmd_register = 0;
    if (dev->master)
        port_out_b(dev->device_port, 0xa0);
    else
        port_out_b(dev->device_port, 0xb0);

    port_out_b(dev->sector_count_port, 0);
    port_out_b(dev->lba_low_port, 0);
    port_out_b(dev->lba_mid_port, 0);
    port_out_b(dev->lba_hi_port, 0);

    /* Identify command */
    port_out_b(dev->command_port, 0xEC);

    if (!port_in_b(dev->command_port)) {
        dev->exists = 0;
        kprint(KPRN_INFO, "ide: No device found!");
        return;
    } else {
        int timeout = 0;
        while (port_in_b(dev->command_port) & 0b10000000) {
            if (++timeout == 100000) {
                dev->exists = 0;
                kprint(KPRN_WARN, "ide: Drive detection timed out. Skipping drive!");
                return;
            }
        }
    }

    /* check for non-standard ATAPI */
    if (port_in_b(dev->lba_mid_port) || port_in_b(dev->lba_hi_port)) {
        dev->exists = 0;
        kprint(KPRN_INFO, "ide: Non-standard ATAPI, ignoring.");
        return;
    }

    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = port_in_b(dev->command_port);
        if (status & 0b00000001) {
            dev->exists = 0;
            kprint(KPRN_ERR, "ide: Error occured!");
            return;
        }
        if (status & 0b00001000) goto success;
    }
    dev->exists = 0;
    kprint(KPRN_WARN, "ide: drive detection timed out. Skipping drive!");
    return;

success:

    kprint(KPRN_INFO, "ide: Storing IDENTIFY info...");
    for (int i = 0; i < 256; i++)
        dev->identify[i] = port_in_w(dev->data_port);

    dev->prdt_phys = (uint32_t)(size_t)pmm_allocz(1);
    dev->prdt = (struct prdt_t *)((size_t)dev->prdt_phys + MEM_PHYS_OFFSET);
    dev->prdt->buffer_phys = (uint32_t)(size_t)pmm_allocz(1);
    dev->prdt_cache = (uint8_t *)((size_t)dev->prdt->buffer_phys + MEM_PHYS_OFFSET);
    dev->prdt->transfer_size = BYTES_PER_SECT;
    dev->prdt->mark_end = 0x8000;
    dev->cache = kalloc(MAX_CACHED_SECTORS * sizeof(cached_sector_t));

    cmd_register = pci_read_device(pci, 0x4);
    if (!(cmd_register & (1 << 2))) {
        cmd_register |= (1 << 2);
        pci_write_device(pci, 0x4, cmd_register);
    }
    memcpy(&dev->sector_count, &dev->identify[100], sizeof(uint64_t));
    kprint(KPRN_INFO, "ide: Sector count: %u", dev->sector_count);

    kprint(KPRN_INFO, "ide: Device successfully identified!");

    dev->exists = 1;

    return;
}

static int ide_read28(int disk, uint32_t sector, uint8_t *buffer) {
    ide_device *dev = &ide_devices[disk];
    port_out_b(dev->bmr_command, 0);
    port_out_d(dev->bmr_prdt, dev->prdt_phys);
    uint8_t bmr_status = port_in_b(dev->bmr_status);
    port_out_b(dev->bmr_status, bmr_status | 0x4 | 0x2);
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0xE0 | ((sector & 0x0F000000) >> 24));
    else
        port_out_b(ide_devices[disk].device_port, 0xF0 | ((sector & 0x0F000000) >> 24));

    port_out_b(ide_devices[disk].sector_count_port, 1);
    port_out_b(ide_devices[disk].lba_low_port, sector & 0x000000FF);
    port_out_b(ide_devices[disk].lba_mid_port, (sector & 0x0000FF00) >> 8);
    port_out_b(ide_devices[disk].lba_hi_port, (sector & 0x00FF0000) >> 16);

    port_out_b(ide_devices[disk].command_port, 0xC8); // READ_DMA command
    port_out_b(dev->bmr_command, 0x8 | 0x1);

    uint8_t status = port_in_b(ide_devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);
    port_out_b(dev->bmr_command, 0);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error reading sector %u on drive %u", sector, disk);
        return -1;
    }

    memcpy(buffer, dev->prdt_cache, BYTES_PER_SECT);
    return 0;
}

static int ide_read48(int disk, uint64_t sector, uint8_t *buffer) {
    ide_device *dev = &ide_devices[disk];
    port_out_b(dev->bmr_command, 0);
    port_out_d(dev->bmr_prdt, dev->prdt_phys);
    uint8_t bmr_status = port_in_b(dev->bmr_status);
    port_out_b(dev->bmr_status, bmr_status | 0x4 | 0x2);
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0x40);
    else
        port_out_b(ide_devices[disk].device_port, 0x50);

    port_out_b(ide_devices[disk].sector_count_port, 0);   // sector count high byte
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    port_out_b(ide_devices[disk].sector_count_port, 1);   // sector count low byte
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)(sector & 0x000000000000FF));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000000000FF00) >> 8));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00000000FF0000) >> 16));

    port_out_b(ide_devices[disk].command_port, 0x25); // READ_DMA command
    port_out_b(dev->bmr_command, 0x8 | 0x1);

    uint8_t status = port_in_b(ide_devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);
    port_out_b(dev->bmr_command, 0);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error reading sector %U on drive %u", sector, disk);
        return -1;
    }

    memcpy(buffer, dev->prdt_cache, BYTES_PER_SECT);
    return 0;
}

static int ide_write28(int disk, uint32_t sector, uint8_t *buffer) {
    ide_device *dev = &ide_devices[disk];
    port_out_b(dev->bmr_command, 0);
    port_out_d(dev->bmr_prdt, dev->prdt_phys);
    uint8_t bmr_status = port_in_b(dev->bmr_status);
    port_out_b(dev->bmr_status, bmr_status | 0x4 | 0x2);
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0xE0 | ((sector & 0x0F000000) >> 24));
    else
        port_out_b(ide_devices[disk].device_port, 0xF0 | ((sector & 0x0F000000) >> 24));

    /* copy buffer to dma area */
    memcpy(dev->prdt_cache, buffer, BYTES_PER_SECT);

    port_out_b(ide_devices[disk].sector_count_port, 1);
    port_out_b(ide_devices[disk].lba_low_port, sector & 0x000000FF);
    port_out_b(ide_devices[disk].lba_mid_port, (sector & 0x0000FF00) >> 8);
    port_out_b(ide_devices[disk].lba_hi_port, (sector & 0x00FF0000) >> 16);

    port_out_b(ide_devices[disk].command_port, 0xCA); // WRITE_DMA command
    port_out_b(dev->bmr_command, 0x1);

    uint8_t status = port_in_b(ide_devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);
    port_out_b(dev->bmr_command, 0);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error writing sector %u on drive %u", sector, disk);
        return -1;
    }

    ide_flush(disk);

    return 0;
}

static int ide_write48(int disk, uint64_t sector, uint8_t *buffer) {
    ide_device *dev = &ide_devices[disk];
    port_out_b(dev->bmr_command, 0);
    port_out_d(dev->bmr_prdt, dev->prdt_phys);
    uint8_t bmr_status = port_in_b(dev->bmr_status);
    port_out_b(dev->bmr_status, bmr_status | 0x4 | 0x2);
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0x40);
    else
        port_out_b(ide_devices[disk].device_port, 0x50);

    /* copy buffer to dma area */
    memcpy(dev->prdt_cache, buffer, BYTES_PER_SECT);

    /* Sector count high byte */
    port_out_b(ide_devices[disk].sector_count_port, 0);
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    /* Sector count lo byte */
    port_out_b(ide_devices[disk].sector_count_port, 1);
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)(sector & 0x000000000000FF));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000000000FF00) >> 8));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00000000FF0000) >> 16));

    port_out_b(ide_devices[disk].command_port, 0x35); // WRITE_DMA command
    port_out_b(dev->bmr_command, 0x1);

    uint8_t status = port_in_b(ide_devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);
    port_out_b(dev->bmr_command, 0);


    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error writing sector %U on drive %u", sector, disk);
        return -1;
    }

    ide_flush_ext(disk);

    return 0;
}

static int ide_flush(int disk) {
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0xE0);
    else
        port_out_b(ide_devices[disk].device_port, 0xF0);

    /* Cache flush command */
    port_out_b(ide_devices[disk].command_port, 0xE7);

    uint8_t status = port_in_b(ide_devices[disk].command_port);

    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error occured while flushing cache.");
        return -1;
    }

    return 0;

}

static int ide_flush_ext(int disk) {
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0x40);
    else
        port_out_b(ide_devices[disk].device_port, 0x50);

    /* Cache flush EXT command */
    port_out_b(ide_devices[disk].command_port, 0xEA);

    uint8_t status = port_in_b(ide_devices[disk].command_port);

    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error occured while flushing cache.");
        return -1;
    }

    return 0;
}
