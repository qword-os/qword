#ifndef __AHCI_MMIO_H__
#define __AHCI_MMIO_H__

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

#define AHCI_CLASS      0x01
#define AHCI_SUBCLASS   0x06
#define AHCI_PROG_IF    0x01


#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

enum fis_type {
    FIS_TYPE_REG_H2D    = 0x27,
    FIS_TYPE_REG_D2H    = 0x34,
    FIS_TYPE_DMA_ACT    = 0x39,
    FIS_TYPE_DMA_SETUP  = 0x41,
    FIS_TYPE_DATA       = 0x46,
    FIS_TYPE_BIST       = 0x58,
    FIS_TYPE_PIO_SETUP  = 0x5F,
    FIS_TYPE_DEV_BITS   = 0xA1,
};

struct fis_regh2d_t {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;        // Reserved
    uint8_t  c:1;       // 1: Command, 0: Control
    uint8_t  command;   // Command register
    uint8_t  featurel;  // Feature register, 7:0
    uint8_t  lba0;      // LBA low register, 7:0
    uint8_t  lba1;      // LBA mid register, 15:8
    uint8_t  lba2;      // LBA high register, 23:16
    uint8_t  device;        // Device register

    // DWORD 2
    uint8_t  lba3;      // LBA register, 31:24
    uint8_t  lba4;      // LBA register, 39:32
    uint8_t  lba5;      // LBA register, 47:40
    uint8_t  featureh;  // Feature register, 15:8

    // DWORD 3
    uint8_t  countl;        // Count register, 7:0
    uint8_t  counth;        // Count register, 15:8
    uint8_t  icc;       // Isochronous command completion
    uint8_t  control;   // Control register

    // DWORD 4
    uint8_t  rsv1[4];   // Reserved
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
    uint8_t  fis_type;  // FIS_TYPE_DATA

    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:4;        // Reserved

    uint8_t  rsv1[2];   // Reserved

    // DWORD 1 ~ N
    uint32_t data[1];   // Payload
};

struct fis_pio_setup_t {
    // DWORD 0
    uint8_t  fis_type;  // FIS_TYPE_PIO_SETUP

    uint8_t  pmport:4;  // Port multiplier
    uint8_t  rsv0:1;        // Reserved
    uint8_t  d:1;       // Data transfer direction, 1 - device to host
    uint8_t  i:1;       // Interrupt bit
    uint8_t  rsv1:1;

    uint8_t  status;        // Status register
    uint8_t  error;     // Error register

    // DWORD 1
    uint8_t  lba0;      // LBA low register, 7:0
    uint8_t  lba1;      // LBA mid register, 15:8
    uint8_t  lba2;      // LBA high register, 23:16
    uint8_t  device;        // Device register

    // DWORD 2
    uint8_t  lba3;      // LBA register, 31:24
    uint8_t  lba4;      // LBA register, 39:32
    uint8_t  lba5;      // LBA register, 47:40
    uint8_t  rsv2;      // Reserved

    // DWORD 3
    uint8_t  countl;        // Count register, 7:0
    uint8_t  counth;        // Count register, 15:8
    uint8_t  rsv3;      // Reserved
    uint8_t  e_status;  // New value of status register

    // DWORD 4
    uint16_t tc;        // Transfer count
    uint8_t  rsv4[2];   // Reserved
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
    uint32_t clb;       // 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;      // 0x04, command list base address upper 32 bits
    uint32_t fb;        // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;       // 0x0C, FIS base address upper 32 bits
    uint32_t is;        // 0x10, interrupt status
    uint32_t ie;        // 0x14, interrupt enable
    uint32_t cmd;       // 0x18, command and status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, task file data
    uint32_t sig;       // 0x24, signature
    uint32_t ssts;      // 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;      // 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;      // 0x30, SATA error (SCR1:SError)
    uint32_t sact;      // 0x34, SATA active (SCR3:SActive)
    uint32_t ci;        // 0x38, command issue
    uint32_t sntf;      // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;       // 0x40, FIS-based switch control
    uint32_t rsv1[11];  // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4]; // 0x70 ~ 0x7F, vendor specific
};

struct hba_mem_t {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;       // 0x00, Host capability
    uint32_t ghc;       // 0x04, Global host control
    uint32_t is;        // 0x08, Interrupt status
    uint32_t pi;        // 0x0C, Port implemented
    uint32_t vs;        // 0x10, Version
    uint32_t ccc_ctl;   // 0x14, Command completion coalescing control
    uint32_t ccc_pts;   // 0x18, Command completion coalescing ports
    uint32_t em_loc;        // 0x1C, Enclosure management location
    uint32_t em_ctl;        // 0x20, Enclosure management control
    uint32_t cap2;      // 0x24, Host capabilities extended
    uint32_t bohc;      // 0x28, BIOS/OS handoff control and status
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

#endif
