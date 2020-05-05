#include <stdint.h>
#include <stddef.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <fs/devfs/devfs.h>
#include <sys/pci.h>
#include <mm/mm.h>
#include <lib/errno.h>
#include <lib/part.h>
#include <lib/cstring.h>
#include <lib/cmem.h>

#define DEVICE_COUNT 4
#define BYTES_PER_SECT 512

#define SECTORS_PER_BLOCK 100
#define BYTES_PER_BLOCK (SECTORS_PER_BLOCK * BYTES_PER_SECT)

#define MAX_CACHED_BLOCKS 8192

#define CACHE_NOT_READY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2

typedef struct {
    uint8_t *cache;
    uint64_t block;
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

    uint32_t bar4;
    uint32_t bmr_command;
    uint32_t bmr_status;
    uint32_t bmr_prdt;
    struct prdt_t *prdt;
    uint32_t prdt_phys;
    uint8_t *prdt_cache;

    cached_sector_t *cache;
    int overwritten_slot;
} ide_device;

static const char *ide_basename = "ide";

static const uint16_t ide_ports[] = { 0x1f0, 0x1f0, 0x170, 0x170 };
static const int max_ports = 4;

static ide_device init_ide_device(uint16_t port_base, int master,
        struct pci_device_t *pci);
static void ide_identify(ide_device* dev, struct pci_device_t *pci);
static int ide_read48(int disk, uint64_t sector, uint16_t count, uint8_t *buffer);
static int ide_write48(int disk, uint64_t sector, uint16_t count, uint8_t *buffer);

static ide_device ide_devices[DEVICE_COUNT];

static lock_t ide_lock = new_lock;

static int find_block(int drive, uint64_t block) {
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((ide_devices[drive].cache[i].block == block)
            && (ide_devices[drive].cache[i].status))
            return i;

    return -1;

}

static int cache_block(int drive, uint64_t block) {
    int targ;
    int ret;

    /* Find empty sector */
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!ide_devices[drive].cache[targ].status) goto fnd;

    /* Slot not find, overwrite another */
    if (ide_devices[drive].overwritten_slot == MAX_CACHED_BLOCKS)
        ide_devices[drive].overwritten_slot = 0;

    targ = ide_devices[drive].overwritten_slot++;

    /* Flush device cache */
    if (ide_devices[drive].cache[targ].status == CACHE_DIRTY) {
        ret = ide_write48(drive, ide_devices[drive].cache[targ].block * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK, ide_devices[drive].cache[targ].cache);

        if (ret == -1) return -1;
    }

    goto notfnd;

fnd:
    /* Allocate some cache for this device */
    ide_devices[drive].cache[targ].cache = kalloc(BYTES_PER_BLOCK);

notfnd:

    /* Load sector into cache */
    ret = ide_read48(drive, block * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK, ide_devices[drive].cache[targ].cache);

    if (ret == -1) return -1;

    ide_devices[drive].cache[targ].block = block;
    ide_devices[drive].cache[targ].status = CACHE_READY;

    return targ;
}

static int ide_read(int drive, void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&ide_lock);

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the sector */
        uint64_t block = (loc + progress) / BYTES_PER_BLOCK;
        int slot = find_block(drive, block);
        if (slot == -1) {
            slot = cache_block(drive, block);
            if (slot == -1) {
                spinlock_release(&ide_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

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
        uint64_t block = (loc + progress) / BYTES_PER_BLOCK;
        int slot = find_block(drive, block);
        if (slot == -1) {
            slot = cache_block(drive, block);
            if (slot == -1) {
                spinlock_release(&ide_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

        memcpy(&ide_devices[drive].cache[slot].cache[offset], buf + progress, chunk);
        ide_devices[drive].cache[slot].status = CACHE_DIRTY;
        progress += chunk;
    }

    spinlock_release(&ide_lock);
    return (int)count;
}

static int ide_flush(int device) {
    spinlock_acquire(&ide_lock);

    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (ide_devices[device].cache[i].status == CACHE_DIRTY) {
            int ret;

            ret = ide_write48(device, ide_devices[device].cache[i].block * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK, ide_devices[device].cache[i].cache);

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

    struct pci_device_t *pci_device;
    // TODO figure out correct prog if and define these values elsewhere
    pci_device = pci_get_device(0x1, 0x1, 0x80, 0);
    if (!pci_device) {
        kprint(KPRN_INFO, "ide: could not find pci device!");
        return;
    }

    int j = 0;
    int master = 1;
    for (int i = 0; i < DEVICE_COUNT; i++) {
        if (j >= max_ports) return;
        while (!(ide_devices[i] = init_ide_device(ide_ports[j], master,
                        pci_device)).exists) {
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
        device.calls.flush = ide_flush;
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

    dev.bar4 = pci_read_device_dword(pci, 0x20);
    if (dev.bar4 & 0x1)
        dev.bar4 &= 0xFFFFFFFC;
    dev.bmr_command = dev.bar4;
    dev.bmr_status = dev.bar4 + 0x2;
    dev.bmr_prdt = dev.bar4 + 0x4;

    ide_identify(&dev, pci);

    return dev;
}

static void ide_identify(ide_device* dev, struct pci_device_t *pci) {
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
    dev->prdt->buffer_phys = (uint32_t)(size_t)pmm_allocz(DIV_ROUNDUP(BYTES_PER_BLOCK, PAGE_SIZE));
    dev->prdt_cache = (uint8_t *)((size_t)dev->prdt->buffer_phys + MEM_PHYS_OFFSET);
    dev->prdt->transfer_size = BYTES_PER_BLOCK;
    dev->prdt->mark_end = 0x8000;
    dev->cache = kalloc(MAX_CACHED_BLOCKS * sizeof(cached_sector_t));

    uint32_t cmd_register = pci_read_device_dword(pci, 0x4);
    if (!(cmd_register & (1 << 2))) {
        cmd_register |= (1 << 2);
        pci_write_device_dword(pci, 0x4, cmd_register);
    }
    memcpy(&dev->sector_count, &dev->identify[100], sizeof(uint64_t));
    kprint(KPRN_INFO, "ide: Sector count: %u", dev->sector_count);

    kprint(KPRN_INFO, "ide: Device successfully identified!");

    dev->exists = 1;
}

static int ide_read48(int disk, uint64_t sector, uint16_t count, uint8_t *buffer) {
    ide_device *dev = &ide_devices[disk];
    port_out_b(dev->bmr_command, 0);
    port_out_d(dev->bmr_prdt, dev->prdt_phys);
    uint8_t bmr_status = port_in_b(dev->bmr_status);
    port_out_b(dev->bmr_status, bmr_status | 0x4 | 0x2);
    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0x40);
    else
        port_out_b(ide_devices[disk].device_port, 0x50);

    port_out_b(ide_devices[disk].sector_count_port, (uint8_t)(count >> 8));   // sector count high byte
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    port_out_b(ide_devices[disk].sector_count_port, (uint8_t)(count & 0xff));   // sector count low byte
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

    memcpy(buffer, dev->prdt_cache, BYTES_PER_BLOCK);
    return 0;
}

static int ide_write48(int disk, uint64_t sector, uint16_t count, uint8_t *buffer) {
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
    memcpy(dev->prdt_cache, buffer, BYTES_PER_BLOCK);

    /* Sector count high byte */
    port_out_b(ide_devices[disk].sector_count_port, (uint8_t)(count >> 8));
    port_out_b(ide_devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(ide_devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(ide_devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    /* Sector count lo byte */
    port_out_b(ide_devices[disk].sector_count_port, (uint8_t)(count & 0xff));
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

    if (ide_devices[disk].master)
        port_out_b(ide_devices[disk].device_port, 0x40);
    else
        port_out_b(ide_devices[disk].device_port, 0x50);

    /* Cache flush EXT command */
    port_out_b(ide_devices[disk].command_port, 0xEA);

    status = port_in_b(ide_devices[disk].command_port);

    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(ide_devices[disk].command_port);

    if (status & 0x01) {
        kprint(KPRN_ERR, "ide: Error occured while flushing cache.");
        return -1;
    }

    return 0;
}
