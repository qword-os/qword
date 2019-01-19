#include <stdint.h>
#include <stddef.h>
#include <ahci.h>
#include <pci.h>
#include <klib.h>
#include <dev.h>

#define BYTES_PER_SECT 512

#define SECTORS_PER_BLOCK 128
#define BYTES_PER_BLOCK (SECTORS_PER_BLOCK * BYTES_PER_SECT)

#define MAX_CACHED_BLOCKS 8192

#define MAX_AHCI_DEVICES 32

#define CACHE_NOT_READY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2

#define SATA_SIG_ATA 0x00000101 /* SATA drive */
#define SATA_SIG_ATAPI 0xeB140101 /* ATAPI drive */
#define SATA_SIG_SEMB 0xc33C0101 /* Enclosure management bridge */
#define SATA_SIG_PM 0x96690101 /* Port multiplier */

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

/** fis **/

enum fis_type {
    FIS_TYPE_REG_H2D	= 0x27,
    FIS_TYPE_REG_D2H	= 0x34,
    FIS_TYPE_DMA_ACT	= 0x39,
    FIS_TYPE_DMA_SETUP	= 0x41,
    FIS_TYPE_DATA		= 0x46,
    FIS_TYPE_BIST		= 0x58,
    FIS_TYPE_PIO_SETUP	= 0x5F,
    FIS_TYPE_DEV_BITS	= 0xA1,
};

struct fis_regh2d_t {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;		// Reserved
    uint8_t  c:1;		// 1: Command, 0: Control
    uint8_t  command;	// Command register
    uint8_t  featurel;	// Feature register, 7:0
    uint8_t  lba0;		// LBA low register, 7:0
    uint8_t  lba1;		// LBA mid register, 15:8
    uint8_t  lba2;		// LBA high register, 23:16
    uint8_t  device;		// Device register

    // DWORD 2
    uint8_t  lba3;		// LBA register, 31:24
    uint8_t  lba4;		// LBA register, 39:32
    uint8_t  lba5;		// LBA register, 47:40
    uint8_t  featureh;	// Feature register, 15:8

    // DWORD 3
    uint8_t  countl;		// Count register, 7:0
    uint8_t  counth;		// Count register, 15:8
    uint8_t  icc;		// Isochronous command completion
    uint8_t  control;	// Control register

    // DWORD 4
    uint8_t  rsv1[4];	// Reserved
};

struct fis_regd2h_t {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_REG_D2H

    uint8_t  pmport:4;    // Port multiplier
    uint8_t  rsv0:2;      // Reserved
    uint8_t  i:1;         // Interrupt bit
    uint8_t  rsv1:1;      // Reserved

    uint8_t  status;      // Status register
    uint8_t  error;       // Error register

    // DWORD 1
    uint8_t  lba0;        // LBA low register, 7:0
    uint8_t  lba1;        // LBA mid register, 15:8
    uint8_t  lba2;        // LBA high register, 23:16
    uint8_t  device;      // Device register

    // DWORD 2
    uint8_t  lba3;        // LBA register, 31:24
    uint8_t  lba4;        // LBA register, 39:32
    uint8_t  lba5;        // LBA register, 47:40
    uint8_t  rsv2;        // Reserved

    // DWORD 3
    uint8_t  countl;      // Count register, 7:0
    uint8_t  counth;      // Count register, 15:8
    uint8_t  rsv3[2];     // Reserved

    // DWORD 4
    uint8_t  rsv4[4];     // Reserved
};

struct fis_data_t {
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_DATA

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:4;		// Reserved

    uint8_t  rsv1[2];	// Reserved

    // DWORD 1 ~ N
    uint32_t data[1];	// Payload
};

struct fis_pio_setup_t {
    // DWORD 0
    uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP

    uint8_t  pmport:4;	// Port multiplier
    uint8_t  rsv0:1;		// Reserved
    uint8_t  d:1;		// Data transfer direction, 1 - device to host
    uint8_t  i:1;		// Interrupt bit
    uint8_t  rsv1:1;

    uint8_t  status;		// Status register
    uint8_t  error;		// Error register

    // DWORD 1
    uint8_t  lba0;		// LBA low register, 7:0
    uint8_t  lba1;		// LBA mid register, 15:8
    uint8_t  lba2;		// LBA high register, 23:16
    uint8_t  device;		// Device register

    // DWORD 2
    uint8_t  lba3;		// LBA register, 31:24
    uint8_t  lba4;		// LBA register, 39:32
    uint8_t  lba5;		// LBA register, 47:40
    uint8_t  rsv2;		// Reserved

    // DWORD 3
    uint8_t  countl;		// Count register, 7:0
    uint8_t  counth;		// Count register, 15:8
    uint8_t  rsv3;		// Reserved
    uint8_t  e_status;	// New value of status register

    // DWORD 4
    uint16_t tc;		// Transfer count
    uint8_t  rsv4[2];	// Reserved
};

struct fis_dma_setup_t {
    uint8_t  fis_type;
    uint8_t  pmport : 4;
    uint8_t  rsvd0 : 1;
    uint8_t  d : 1;
    uint8_t  i : 1;
    uint8_t  a : 1;
    uint8_t  rsvd1[2];
    uint64_t dma_buffer_id;
    uint32_t rsvd2;
    uint32_t dma_buf_offset;
    uint32_t transfer_count;
    uint32_t rsvd3;
}__attribute((packed));

/** hba **/

struct hba_port_t {
    uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;		// 0x04, command list base address upper 32 bits
    uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
    uint32_t is;		// 0x10, interrupt status
    uint32_t ie;		// 0x14, interrupt enable
    uint32_t cmd;		// 0x18, command and status
    uint32_t rsv0;		// 0x1C, Reserved
    uint32_t tfd;		// 0x20, task file data
    uint32_t sig;		// 0x24, signature
    uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;		// 0x30, SATA error (SCR1:SError)
    uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
    uint32_t ci;		// 0x38, command issue
    uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;		// 0x40, FIS-based switch control
    uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
};

struct hba_mem_t {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;		// 0x00, Host capability
    uint32_t ghc;		// 0x04, Global host control
    uint32_t is;		// 0x08, Interrupt status
    uint32_t pi;		// 0x0C, Port implemented
    uint32_t vs;		// 0x10, Version
    uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
    uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
    uint32_t em_loc;		// 0x1C, Enclosure management location
    uint32_t em_ctl;		// 0x20, Enclosure management control
    uint32_t cap2;		// 0x24, Host capabilities extended
    uint32_t bohc;		// 0x28, BIOS/OS handoff control and status
    uint8_t  rsv[0xA0-0x2C];
    uint8_t  vendor[0x100-0xA0];
    struct hba_port_t ports[];
};

struct hba_cmd_hdr_t {
    uint8_t cfl:5;
    uint8_t a:1;
    uint8_t w:1;
    uint8_t p:1;
    uint8_t r:1;
    uint8_t b:1;
    uint8_t c:1;
    uint8_t rsv0:1;
    uint8_t pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
};

struct hba_prdtl_t {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc : 22;
    uint32_t rsv1 : 9;
    uint32_t i : 1;
};

struct hba_cmd_tbl_t {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    struct hba_prdtl_t prdt_entry[1];
};

struct hba_fis_t {
    struct fis_dma_setup_t dsfis;
    uint8_t pad0[4];

    struct fis_pio_setup_t psfis;
    uint8_t pad1[12];

    struct fis_regd2h_t rfis;
    uint8_t pad2[4];

    uint8_t sdbfis[8];

    uint8_t ufis[64];
    uint8_t rsv[0x60];
};

static int ahci_read(int drive, void *buf, uint64_t loc, size_t count);
static int ahci_write(int drive, const void *buf, uint64_t loc, size_t count);
static int ahci_flush(int device);

static const char *ahci_names[] = {
    "sda", "sdb", "sdc", "sdd",
    "sde", "sdf", "sdg", "sdh",
    "sdi", "sdj", "sdk", "sdl",
    "sdm", "sdn", "sdo", "sdp",
    "sdq", "sdr", "sds", "sdt",
    "sdu", "sdv", "sdw", "sdx",
    "sdy", "sdz", "sdaa", "sdab",
    "sdac", "sdad", "sdae", "sdaf",
    NULL
};

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
        volatile struct hba_port_t *port, uint8_t cmd, size_t portno);

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

void init_ahci(void) {
    /* initialise the device array */
    struct pci_device_t device = {0};
    ahci_devices = kalloc(MAX_AHCI_DEVICES * sizeof(struct ahci_device_t));

    /* Search for an AHCI controller and calculate the base address for
     * AHCI MMIO */
    uint8_t class_mass_storage = 0x01;
    uint8_t subclass_serial_ata = 0x06;
    int ret = pci_get_device(&device, class_mass_storage, subclass_serial_ata);
    if (ret == -1) {
        kprint(KPRN_WARN, "ahci: Failed to find AHCI controller. SATA support unavailable");
        return;
    }

    /* ensure the AHCI controller is the bus master */
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
                /* setup per-port memory structures */
                port_rebase(&ahci_base->ports[i]);
                /* identify a SATA drive and install it as a device */
                int ret = init_ahci_device(&ahci_devices[i], &ahci_base->ports[i], 0xec, i);
                if (ret == -1) {
                    kprint(KPRN_WARN, "failed to initialise sata device at index %u", i);
                } else {
                    device_add(ahci_names[i],
                            i,
                            ahci_devices[i].sector_count * 512,
                            &ahci_read,
                            &ahci_write,
                            &ahci_flush);
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
        volatile struct hba_port_t *port, uint8_t cmd, size_t portno) {
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
    kmemset64((void *)((size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET), 0,
            sizeof(volatile struct hba_cmd_tbl_t) / 8);

    cmdtbl->prdt_entry[0].dba = (uint32_t)(size_t)identify;
    cmdtbl->prdt_entry[0].dbc = 511;
    cmdtbl->prdt_entry[0].i = 1;

    struct fis_regh2d_t *cmdfis = (struct fis_regh2d_t *)(((size_t)cmdtbl->cfis));
    kmemset64((void *)(((size_t)(void *)cmdtbl->cfis)), 0, sizeof(struct fis_regh2d_t) / 8);

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
    kprint(KPRN_INFO, "ahci: Initialised /dev/%s", ahci_names[portno]);

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
    kmemset64((void *)((size_t)port->clb + MEM_PHYS_OFFSET), 0,
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
    kmemset64((void *)((size_t)cmd_hdr->ctba + MEM_PHYS_OFFSET), 0,
            sizeof(volatile struct hba_cmd_tbl_t) / sizeof(uint64_t));

    /* `buf` is guaranteed to be physically contiguous, so we only need one PRDT */
    cmdtbl->prdt_entry[0].dba = (uint32_t)((size_t)buf - MEM_PHYS_OFFSET);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    struct fis_regh2d_t *cmdfis = (struct fis_regh2d_t *)((size_t)cmdtbl->cfis);
    kmemset64(cmdfis, 0, sizeof(struct fis_regh2d_t) / sizeof(uint64_t));

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

static lock_t ahci_lock = 1;

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

        kmemcpy(buf + progress, &ahci_devices[drive].cache[slot].cache[offset], chunk);
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

        kmemcpy(&ahci_devices[drive].cache[slot].cache[offset], buf + progress, chunk);
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
