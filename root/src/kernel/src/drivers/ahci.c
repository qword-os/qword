#include <ahci.h>
#include <ahci/hba.h>
#include <ahci/fis.h>
#include <pci.h>
#include <klib.h>

size_t ahci_base;
struct hba_port_t *hba_ports;

void start_cmd(volatile struct hba_port_t *);
void stop_cmd(volatile struct hba_port_t *);
void kmemset64(void *ptr, int c, size_t count);

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

void init_ahci(void) {
    struct pci_device_t device = {0};

    uint8_t class_mass_storage = 0x01;
    uint8_t subclass_serial_ata = 0x06;
    int ret = pci_get_device(&device, class_mass_storage, subclass_serial_ata);
    if (!ret) kprint(KPRN_INFO, "ahci: Found AHCI controller");

    ahci_base = (size_t)pci_read_device(&device, 0x24);
    kprint(KPRN_DBG, "ahci: ABAR at %x", ahci_base);
    volatile struct hba_mem_t *mem = (volatile struct hba_mem_t *)(ahci_base + MEM_PHYS_OFFSET);

    for (size_t i = 0; i < 32; i++) {
        int ret = probe_port(mem, i);
        switch (ret) {
            case AHCI_DEV_SATA:
                kprint(KPRN_DBG, "found sata device at port index %u", i);
                ret = ahci_init_ata(&mem->ports[i], i);
                if (ret == -1)
                    kprint(KPRN_DBG, "failed to initialise sata device at index %u", i);
                continue;
            case AHCI_DEV_SATAPI:
                ret = ahci_init_atapi(&mem->ports[i], i);
                continue;
            default:
                continue;
        }
    }
}

int probe_port(volatile struct hba_mem_t *mem, size_t portno) {
    uint32_t pi = mem->pi;

    if (pi & 1) {
        return check_type(&mem->ports[portno]);
    }

    return -1;
}

int ahci_init_ata(volatile struct hba_port_t *port, size_t portno) {
    port_rebase(port, portno);
    //kprint(KPRN_DBG, "port rebase done, begin identify command");
    //int ret = ahci_identify(port, 0xec);
    //if (ret == -1)
      //  return -1; 

    return 0;
}

int ahci_init_atapi(volatile struct hba_port_t *port, size_t portno) {
    port_rebase(port, portno);

    return 0;
}

/* Reconfigure the memory areas for a given port */
void port_rebase(volatile struct hba_port_t *port, size_t portno) {
    kprint(KPRN_DBG, "stopping command engine");
    //stop_cmd(port);

    /* calculate base of the command list */
    kprint(KPRN_DBG, "calculating base of command list");
    port->clb = (ahci_base + (portno << 10));
    port->clbu = 0;
    /* zero command list */
    kprint(KPRN_DBG, "zeroing command list");
    kmemset64((void *)(((size_t)port->clb) + MEM_PHYS_OFFSET), 0, 1024);

    /* calculate received fis base addr */
    kprint(KPRN_DBG, "calculating base of fis receive area");
    port->fb  = (ahci_base + (32 << 10) + (portno << 8));
    kprint(KPRN_DBG, "fb = %x", (size_t)(void *)port->fb);
    port->fbu = 0;
    kprint(KPRN_DBG, "zeroing fis receive area");
    /* fis entry size = 256b per port */
    kmemset64((void *)(((size_t)port->fb) + MEM_PHYS_OFFSET), 0, 256);

    volatile struct hba_cmd_hdr_t *cmd_hdr = (volatile struct hba_cmd_hdr_t *)((size_t)port->clb + MEM_PHYS_OFFSET);

    kprint(KPRN_DBG, "setting up command tables");
    for (size_t i = 0; i < 32; i++) {
        cmd_hdr[i].prdtl = 8;

        /* command table base addr = 40K + 8K * portno + header index * 256 */
        cmd_hdr[i].ctba = (ahci_base + (40 << 10) + (portno << 13) + (i << 8));
        cmd_hdr[i].ctbau = 0;
        kmemset64((void *)(((size_t)cmd_hdr[i].ctba) + MEM_PHYS_OFFSET), 0, 256);
    }

    /* restart command engine */
    start_cmd(port);
}

int ahci_identify(volatile struct hba_port_t *port, uint8_t cmd) {
    uint16_t dest[256];
    int spin = 0;

    port->is = (uint32_t)-1;
    
    int slot = find_cmdslot(port);
    if (slot == -1) {
        kprint(KPRN_DBG, "failed to find command slot");
        return -1;
    }

    volatile struct hba_cmd_hdr_t *cmd_hdr = (volatile struct hba_cmd_header_t *)(size_t)port->clb;
    cmd_hdr += slot;
    cmd_hdr->cfl = sizeof(volatile struct fis_regh2d_t) / sizeof(uint32_t);
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;

    volatile struct hba_cmd_tbl_t *cmdtbl = (volatile struct hba_cmd_tbl_t *)(size_t)cmd_hdr->ctba;
    kmemset((void *)cmdtbl, 0, sizeof(volatile struct hba_cmd_tbl_t));

    cmdtbl->prdt_entry[0].dba = (uint32_t)dest;
    cmdtbl->prdt_entry[0].dbc = (512 | 1);

    struct fis_regh2d_t *cmdfis = (struct fis_reg_h2d_t *)(size_t)cmdtbl->cfis;
    kmemset(cmdfis, 0, sizeof(struct fis_regh2d_t));

    cmdfis->pmport = (uint8_t)(1 << 7);
    cmdfis->command = cmd;
    cmdfis->device = 0;
    cmdfis->countl = 1;
    cmdfis->counth = 0;
    cmdfis->fis_type = FIS_TYPE_REG_H2D;

    kprint(KPRN_DBG, "fis setup complete, waiting for a free port");
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }

    if (spin > 1000000) {
        kprint(KPRN_WARN, "ahci: Port hung");
    }

    port->ci = 1 << slot;
    start_cmd(port);

    for (;;) {
        if ((port->ci & (1 << slot)) == 0)
            break;
        /* check for task file error */
        if (port->is & (1 << 30)) {
            kprint(KPRN_DBG, "ahci: Disk read error in ahci_identify()");
            return -1;
        }
    }

    /* Check again for task file error */
    if (port->is & (1 << 30)) {
        kprint(KPRN_DBG, "ahci: Disk read error in ahci_identify()");
        return -1;
    }

    stop_cmd(port);

    kprint(KPRN_DBG, "identify successful");

    return 0;
}

void start_cmd(volatile struct hba_port_t *port) {
    while (port->cmd & HBA_PxCMD_ST);

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;

    return;
}

int find_cmdslot(volatile struct hba_port_t *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < slots; i++) {
        if (!(slots & 1)) {
            return i;
        }

        slots >>= 1;
    }

    return -1;
}

void stop_cmd(volatile struct hba_port_t *port) {
    /* Clear bit 0 */
    port->cmd &= ~HBA_PxCMD_ST;

    for (;;) {
        if (port->cmd & HBA_PxCMD_FR)
            continue;
        if (port->cmd & HBA_PxCMD_CR)
            continue;
        break;
    }

    /* Clear bit 4 */
    port->cmd &= ~HBA_PxCMD_FRE;

    return;
}

void kmemset64(void *ptr, int c, size_t count) {
    uint64_t *p = ptr, *end = p + count;

    for (; p != end; p++) {
        *p = (uint64_t)c;
    }

    return;
}
