#ifndef __AHCI_H__
#define __AHCI_H__

#include <stdint.h>
#include <stddef.h>
#include <ahci/hba.h>

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

struct ahci_device_t {
    volatile struct hba_port_t *port;
    int exists;
    uint64_t sector_count;
};

void init_ahci(void);
int ahci_init_ata(volatile struct hba_port_t *);
int ahci_init_atapi(volatile struct hba_port_t *);
void port_rebase(volatile struct hba_port_t *);

int init_ahci_device(struct ahci_device_t *, volatile struct hba_port_t *, uint8_t, size_t);
int ahci_read(int, void *, uint64_t, size_t);
int ahci_write(int, const void *, uint64_t, size_t);
int ahci_rw(volatile struct hba_port_t *,
            uint32_t, uint32_t,
            uint32_t, uint16_t *, int);
int ahci_flush(int);

int find_cmdslot(volatile struct hba_port_t *);
void start_cmd(volatile struct hba_port_t *);
void stop_cmd(volatile struct hba_port_t *);
int probe_port(volatile struct hba_mem_t *, size_t);

#endif
