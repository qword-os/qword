#include <ahci.h>
#include <ahci/hba.h>
#include <ahci/fis.h>
#include <pci.h>
#include <klib.h>

size_t ahci_base;
struct hba_port_t *hba_ports;

void start_cmd(struct hba_port_t *);
void stop_cmd(struct hba_port_t *);

static int check_type(struct hba_port_t *port) {
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

    ahci_base = (size_t)pci_get_bar(&device, 5);
    struct hba_mem_t *mem = (struct hba_mem_t *)ahci_base;

    for (size_t i = 0; i < 32; i++) {
        int ret = probe_port(mem, i);
        switch (ret) {
            case AHCI_DEV_SATA:
                ret = ahci_init_ata(&mem->ports[i], i);
                continue;
            case AHCI_DEV_SATAPI:
                ret = ahci_init_atapi(&mem->ports[i], i);
                continue;
            default:
                continue;
        }
    }
}

int probe_port(struct hba_mem_t *mem, size_t portno) {
    uint32_t pi = mem->pi;

    if (pi & 1) {
        return check_type(&mem->ports[portno]);
    }

    return -1;
}

int ahci_init_ata(struct hba_port_t *port, size_t portno) {
    port_rebase(port, portno);

    return 0;
}

int ahci_init_atapi(struct hba_port_t *port, size_t portno) {
    port_rebase(port, portno);

    return 0;
}

/* Reconfigure the memory areas for a given port */
void port_rebase(struct hba_port_t *port, size_t portno) {
    stop_cmd(port);

    /* calculate base of the command list */
    port->clb = ahci_base + (portno << 10);
    port->clbu = 0;
    /* zero command list */
    kmemset((void *)(size_t)(port->clb), 0, 1024);

    /* calculate received fis base addr */
    port->fb  = ahci_base + (32 << 10) + (portno << 8);
    port->fbu = 0;
    /* fis entry size = 256b per port */
    kmemset((void *)(size_t)(port->clb), 0, 256);

    struct hba_cmd_hdr_t *cmd_hdr = (struct hba_cmd_hdr_t *)(size_t)port->clb;

    for (size_t i = 0; i < 32; i++) {
        cmd_hdr[i].prdtl = 8;

        /* command table base addr = 40K + 8K * portno + header index * 256 */
        cmd_hdr[i].ctba = ahci_base + (40 << 10) + (portno << 13) + (i << 8);
        cmd_hdr[i].ctbau = 0;
        kmemset((void *)(size_t)(cmd_hdr[i].ctba), 0, 256);
    }

    /* restart command engine */
    start_cmd(port);
}

void start_cmd(struct hba_port_t *port) {
    while (port->cmd & HBA_PxCMD_ST);

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;

    return;
}

void stop_cmd(struct hba_port_t *port) {
    /* Clear bit 0 */
    port->cmd &= ~HBA_PxCMD_ST;

    for (;;) {
        if (port->cmd & HBA_PxCMD_FR)
            continue;
        if (port->cmd & HBA_PxCMD_CR)
            break;
    }

    /* Clear bit 4 */
    port->cmd &= ~HBA_PxCMD_FRE;

    return;
}
