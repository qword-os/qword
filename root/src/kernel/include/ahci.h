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

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

void init_ahci(void);
int probe_port(struct hba_mem_t *, size_t);
int ahci_init_ata(struct hba_port_t *, size_t);
int ahci_init_atapi(struct hba_port_t *, size_t);
void port_rebase(struct hba_port_t *, size_t);

#endif
