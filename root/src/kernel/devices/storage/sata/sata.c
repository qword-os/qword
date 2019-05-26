#include <stdint.h>
#include <stddef.h>
#include "sata_private.h"
#include <misc/pci.h>
#include <lib/klib.h>
#include <fs/devfs/devfs.h>
#include <lib/errno.h>
#include <lib/part.h>

static int ahci_read(int drive, void *buf, uint64_t loc, size_t count);
static int ahci_write(int drive, const void *buf, uint64_t loc, size_t count);
static int ahci_flush(int device);

static const char *sata_basename = "sata";

struct cached_block_t {
    uint8_t *cache;
    uint64_t block;
    int status;
};

struct ahci_device_t {
    volatile struct hba_port_t *port;
    int exists;
    uint64_t sector_count;
    struct cached_block_t *cache;
    int overwritten_slot;
};

static struct ahci_device_t *ahci_devices;

static void port_rebase(volatile struct hba_port_t *port);
static int init_ahci_device(struct ahci_device_t *device,
        volatile struct hba_port_t *port, uint8_t cmd);

static int check_type(volatile struct hba_port_t *port) {
    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0f;
    uint8_t det = ssts & 0x0f;

    /* Check for correct interface and active state */
    if ((ipm != 0x01) || (det != 0x03))
        return AHCI_DEV_NULL;

    switch (port->sig) {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            return AHCI_DEV_PM;
        default:
            return AHCI_DEV_SATA;
    }

    return AHCI_DEV_NULL;
}

static int probe_port(volatile struct hba_mem_t *mem, size_t portno) {
    uint32_t pi = mem->pi;

    if (pi & 1) {
        return check_type(&mem->ports[portno]);
    }

    return -1;
}

static void start_cmd(volatile struct hba_port_t *port) {
    /* manually stop the DMA engine, just a sanity check */
    port->cmd &= ~HBA_PxCMD_ST;

    while (port->cmd & HBA_PxCMD_CR);

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;

    return;
}

static int find_cmdslot(volatile struct hba_port_t *port) {
    /* calculate a list of slots, where a zero bit
     * represents a free slot, and search for the first
     * zero bit */
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < MAX_AHCI_DEVICES; i++) {
        if (!(slots & (1 << i))) {
            return i;
        }
    }

    return -1;
}

static void stop_cmd(volatile struct hba_port_t *port) {
    /* Clear bit 0 */
    port->cmd &= ~HBA_PxCMD_ST;

    while (port->cmd & HBA_PxCMD_CR);

    /* Clear bit 4 */
    port->cmd &= ~HBA_PxCMD_FRE;

    return;
}

void init_dev_sata(void) {
    struct pci_device_t device = {0};
    ahci_devices = kalloc(MAX_AHCI_DEVICES * sizeof(struct ahci_device_t));

    int ret = pci_get_device(&device, AHCI_CLASS, AHCI_SUBCLASS, AHCI_PROG_IF);
    if (ret == -1) {
        kprint(KPRN_WARN, "ahci: Failed to find AHCI controller. SATA support unavailable");
        return;
    }

    uint32_t cmd_register = pci_read_device(&device, 0x4);
    if (!(cmd_register & (1 << 2))) {
        kprint(KPRN_DBG, "enabling busmastering");
        cmd_register |= (1 << 2);
        pci_write_device(&device, 0x4, cmd_register);
    }

    kprint(KPRN_INFO, "ahci: Found AHCI controller");

    volatile struct hba_mem_t *ahci_base =
      (volatile struct hba_mem_t *)((size_t)pci_read_device(&device, 0x24) + MEM_PHYS_OFFSET);
    kprint(KPRN_INFO, "ahci: ABAR at %X", ahci_base);

    for (size_t i = 0; i < MAX_AHCI_DEVICES; i++) {
        switch (probe_port(ahci_base, i)) {
            case AHCI_DEV_SATA:
                kprint(KPRN_INFO, "ahci: Found sata device at port index %u", i);

                // setup device memory structures
                port_rebase(&ahci_base->ports[i]);

                // identify a sata drive
                int ret = init_ahci_device(&ahci_devices[i], &ahci_base->ports[i], 0xec);

                if (ret == -1) {
                    kprint(KPRN_WARN, "failed to initialise sata device at index %u", i);
                } else {
                    struct device_t device = {0};
                    device.calls = default_device_calls;
                    char *dev_name = prefixed_itoa(sata_basename, i, 10);
                    strcpy(device.name, dev_name);
                    kprint(KPRN_INFO, "ahci: Initialised /dev/%s", dev_name);
                    kfree(dev_name);
                    device.intern_fd = i;
                    device.size = ahci_devices[i].sector_count * 512;
                    device.calls.read = ahci_read;
                    device.calls.write = ahci_write;
                    device.calls.flush = ahci_flush;
                    device_add(&device);
                    enum_partitions(dev_name, &device);
                }
                break;
            default:
                break;
        }
    }
}

/* Allocate space for command lists, tables etc for a given port */
static void port_rebase(volatile struct hba_port_t *port) {
    /* allocate an area for the command list */
    port->clb = (uint32_t)(size_t)pmm_allocz(1);
    port->clbu = 0;

    /* Reserve some memory for the hba fis receive area and setup values */
    struct hba_fis_t *hba_fis = kalloc(sizeof(struct hba_fis_t));

    /* set fis types in fis receive area */
    hba_fis->dsfis.fis_type = FIS_TYPE_DMA_SETUP;
    hba_fis->psfis.fis_type = FIS_TYPE_PIO_SETUP;
    hba_fis->rfis.fis_type = FIS_TYPE_REG_D2H;
    hba_fis->sdbfis[0] = FIS_TYPE_DEV_BITS;

    /* Set the address that received FISes will be copied to */
    port->fb  = (uint32_t)((size_t)hba_fis - MEM_PHYS_OFFSET);
    port->fbu = 0;

    volatile struct hba_cmd_hdr_t *cmd_hdr = (volatile struct hba_cmd_hdr_t *)(
            (size_t)port->clb + MEM_PHYS_OFFSET);

    for (size_t i = 0; i < MAX_AHCI_DEVICES; i++) {
        cmd_hdr[i].prdtl = 8;

        /* command table base addr = 40K + 8K * portno + header index * 256 */
        cmd_hdr[i].ctba = (uint32_t)(size_t)pmm_allocz(1);
        cmd_hdr[i].ctbau = 0;
    }

    return;
}

static int init_ahci_device(struct ahci_device_t *device,
        volatile struct hba_port_t *port, uint8_t cmd) {
    uint16_t *identify = pmm_allocz(1);
    int spin = 0;

    int slot = find_cmdslot(port);
    if (slot == -1) {
        kprint(KPRN_WARN, "ahci: failed to find command slot for identify command");
        return -1;
    }

    volatile struct hba_cmd_hdr_t *cmd_hdr = (volatile struct hba_cmd_hdr_t *)(
            (size_t)port->clb + MEM_PHYS_OFFSET);

    /* Setup command header */
    cmd_hdr += slot;
    cmd_hdr->cfl = sizeof(volatile struct fis_regh2d_t) / sizeof(uint32_t);
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;

    /* construct a command table and populate it */
    volatile struct hba_cmd_tbl_t *cmdtbl = (volatile struct hba_cmd_tbl_t *)(
            ((size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET));
    memset64((void *)((size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET), 0,
            sizeof(volatile struct hba_cmd_tbl_t) / 8);

    cmdtbl->prdt_entry[0].dba = (uint32_t)(size_t)identify;
    cmdtbl->prdt_entry[0].dbc = 511;
    cmdtbl->prdt_entry[0].i = 1;

    struct fis_regh2d_t *cmdfis = (struct fis_regh2d_t *)(((size_t)cmdtbl->cfis));
    memset64((void *)(((size_t)(void *)cmdtbl->cfis)), 0, sizeof(struct fis_regh2d_t) / 8);

    cmdfis->command = cmd;
    cmdfis->c = 1;
    cmdfis->device = 0;
    cmdfis->pmport = 0;
    cmdfis->fis_type = FIS_TYPE_REG_H2D;

    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }

    if (spin >= 1000000) {
        kprint(KPRN_WARN, "ahci: Port hung");
    }

    start_cmd(port);
    port->ci = 1 << slot;

    for (;;) {
        if (!(port->ci & (1 << slot))) {
            break;
        }
        /* check for task file error */
        if (port->is & (1 << 30)) {
            kprint(KPRN_WARN, "ahci: Disk read error in ahci_identify()");
            return -1;
        }
    }

    if (port->is & (1 << 30)) {
        kprint(KPRN_WARN, "ahci: Disk read error in ahci_identify()");
        return -1;
    }

    stop_cmd(port);

    /* Setup the device for use with the kernel device subsystem */
    device->exists = 1;
    device->port = port;
    device->sector_count = *((uint64_t *)((size_t)&identify[100] + MEM_PHYS_OFFSET));
    device->cache = kalloc(MAX_CACHED_BLOCKS * sizeof(struct cached_block_t));

    kprint(KPRN_INFO, "ahci: Sector count = %U", device->sector_count);
    kprint(KPRN_INFO, "ahci: Identify successful");

    pmm_free(identify, 1);

    return 0;
}

/* This wrapper function performs both read and write DMA operations - see ahci_read
 * and ahci_write */
static int ahci_rw(volatile struct hba_port_t *port,
        uint64_t start, uint32_t count, uint16_t *buf, int w) {
    uint32_t startl = (uint32_t)start;
    uint32_t starth = start >> 32;

    int spin = 0;

    int slot = find_cmdslot(port);
    if (slot == -1) {
        return -1;
    }

    volatile struct hba_cmd_hdr_t *cmd_hdr = (volatile struct hba_cmd_hdr_t *)(
            (size_t)port->clb + MEM_PHYS_OFFSET);
    memset64((void *)((size_t)port->clb + MEM_PHYS_OFFSET), 0,
            sizeof(volatile struct hba_cmd_hdr_t) / 8);

    cmd_hdr += slot;
    cmd_hdr->cfl = sizeof(struct fis_regh2d_t) / sizeof(uint32_t);
    if (w)
        cmd_hdr->w = 1;
    else
        cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;

    volatile struct hba_cmd_tbl_t *cmdtbl = (volatile struct hba_cmd_tbl_t *)(
            (size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET);
    /* ensure all entries in the command table are initialised to 0 */
    memset64((void *)((size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET), 0,
            sizeof(volatile struct hba_cmd_tbl_t) / sizeof(uint64_t));

    /* `buf` is guaranteed to be physically contiguous, so we only need one PRDT */
    cmdtbl->prdt_entry[0].dba = (uint32_t)((size_t)buf - MEM_PHYS_OFFSET);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    struct fis_regh2d_t *cmdfis = (struct fis_regh2d_t *)((size_t)cmdtbl->cfis);
    memset64(cmdfis, 0, sizeof(struct fis_regh2d_t) / sizeof(uint64_t));

    /* Setup the command FIS according to whether the command is read or
     * write */
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    if (w)
        /* Assume support for LBA48 commands - QEMU supports this
         * but it's not universal */
        cmdfis->command = ATA_CMD_WRITE_DMA_EXT;
    else
        cmdfis->command = ATA_CMD_READ_DMA_EXT;

    cmdfis->lba0 = (uint8_t)(startl & 0x000000000000ff);
    cmdfis->lba1 = (uint8_t)((startl & 0x0000000000ff00) >> 8);
    cmdfis->lba2 = (uint8_t)((startl & 0x00000000ff0000) >> 16);

    /* again, this assumes lba48 */
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)((startl & 0x000000ff000000) >> 24);
    cmdfis->lba4 = (uint8_t)((starth & 0x0000ff00000000));
    cmdfis->lba5 = (uint8_t)((starth & 0x00ff0000000000) >> 8);

    cmdfis->countl = count & 0xff;
    cmdfis->counth = (count >> 8) & 0xff;

    /* Now wait for command completion */
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }

    if (spin >= 1000000) {
        kprint(KPRN_WARN, "ahci: Port hung");
        return -1;
    }

    start_cmd(port);
    port->ci = 1 << slot;

    for (;;) {
        /* Wait for ci to clear */
        if (!(port->ci & (1 << slot))) {
            break;
        }

        /* check for task file error */
        if (port->is & (1 << 30)) {
            kprint(KPRN_WARN, "ahci: Disk I/O error in ahci_rw");
            return -1;
        }
    }

    if (port->is & (1 << 30)) {
        kprint(KPRN_WARN, "ahci: Disk I/O error in ahci_rw");
        return -1;
    }

    stop_cmd(port);

    return ((int)count * 512);
}

static lock_t ahci_lock = new_lock;

static int find_block(int drive, uint64_t block) {
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((ahci_devices[drive].cache[i].block == block)
            && (ahci_devices[drive].cache[i].status))
            return i;

    return -1;

}

static int cache_block(int drive, uint64_t block) {
    int targ;
    int ret;

    /* Find empty sector */
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!ahci_devices[drive].cache[targ].status) goto fnd;

    /* Slot not find, overwrite another */
    if (ahci_devices[drive].overwritten_slot == MAX_CACHED_BLOCKS)
        ahci_devices[drive].overwritten_slot = 0;

    targ = ahci_devices[drive].overwritten_slot++;

    /* Flush device cache */
    if (ahci_devices[drive].cache[targ].status == CACHE_DIRTY) {
        ret = ahci_rw(ahci_devices[drive].port,
            ahci_devices[drive].cache[targ].block * SECTORS_PER_BLOCK,
            SECTORS_PER_BLOCK, (void *)ahci_devices[drive].cache[targ].cache, 1);

        if (ret == -1) return -1;
    }

    goto notfnd;

fnd:
    /* Allocate some cache for this device */
    ahci_devices[drive].cache[targ].cache = kalloc(BYTES_PER_BLOCK);

notfnd:

    /* Load sector into cache */
    ret = ahci_rw(ahci_devices[drive].port,
        block * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK,
        (void *)ahci_devices[drive].cache[targ].cache, 0);

    if (ret == -1) return -1;

    ahci_devices[drive].cache[targ].block = block;
    ahci_devices[drive].cache[targ].status = CACHE_READY;

    return targ;
}

static int ahci_read(int drive, void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&ahci_lock);

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the sector */
        uint64_t block = (loc + progress) / BYTES_PER_BLOCK;
        int slot = find_block(drive, block);
        if (slot == -1) {
            slot = cache_block(drive, block);
            if (slot == -1) {
                spinlock_release(&ahci_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

        memcpy(buf + progress, &ahci_devices[drive].cache[slot].cache[offset], chunk);
        progress += chunk;
    }

    spinlock_release(&ahci_lock);
    return (int)count;
}

static int ahci_write(int drive, const void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&ahci_lock);

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the sector */
        uint64_t block = (loc + progress) / BYTES_PER_BLOCK;
        int slot = find_block(drive, block);
        if (slot == -1) {
            slot = cache_block(drive, block);
            if (slot == -1) {
                spinlock_release(&ahci_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

        memcpy(&ahci_devices[drive].cache[slot].cache[offset], buf + progress, chunk);
        ahci_devices[drive].cache[slot].status = CACHE_DIRTY;
        progress += chunk;
    }

    spinlock_release(&ahci_lock);
    return (int)count;
}

static int ahci_flush(int device) {
    spinlock_acquire(&ahci_lock);

    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (ahci_devices[device].cache[i].status == CACHE_DIRTY) {
            int ret;

            ret = ahci_rw(ahci_devices[device].port,
                ahci_devices[device].cache[i].block * SECTORS_PER_BLOCK,
                SECTORS_PER_BLOCK, (void *)ahci_devices[device].cache[i].cache, 1);

            if (ret == -1) {
                spinlock_release(&ahci_lock);
                return -1;
            }

            ahci_devices[device].cache[i].status = CACHE_READY;
        }
    }

    spinlock_release(&ahci_lock);
    return 0;
}
