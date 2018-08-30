#ifndef __AHCI_HBA_H__
#define __AHCI_HBA_H__

#include <stdint.h>
#include <stddef.h>
#include <ahci/fis.h>

typedef volatile struct {
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
} hba_port_t;

typedef volatile struct {
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
    hba_port_t ports[1];
} hba_mem_t;

typedef volatile struct {
    // DW0
    uint8_t cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
    uint8_t a:1;		// ATAPI
    uint8_t w:1;		// Write, 1: H2D, 0: D2H
    uint8_t p:1;		// Prefetchable

    uint8_t r:1;		// Reset
    uint8_t b:1;		// BIST
    uint8_t c:1;		// Clear busy upon R_OK
    uint8_t rsv0:1;		// Reserved
    uint8_t pmp:4;		// Port multiplier port

    uint16_t prdtl;		// Physical region descriptor table length in entries

    // DW1
    volatile uint32_t prdbc;		// Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t ctba;		// Command table descriptor base address
    uint32_t ctbau;		// Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t rsv1[4];	// Reserved
} hba_cmd_hdr_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc : 22;
    uint32_t rsv1 : 9;
    uint32_t i : 1;
} hba_prdt_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_t	prdt_entry[1];
} hba_cmd_tbl_t;


#endif
