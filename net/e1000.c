#include <net/e1000.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <lib/alloc.h>
#include <sys/pci.h>
#include <sys/panic.h>

#define INTEL_VEND 0x8086  // Vendor ID for Intel.

// Registers.
#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_CTRL_EXT    0x0018
#define REG_IMASK       0x00d0
#define REG_RCTRL       0x0100
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818
#define REG_TCTRL       0x0400
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818
#define REG_RDTR        0x2820 // RX Delay Timer Register.
#define REG_RXDCTL      0x3828 // RX Descriptor Control.
#define REG_RADV        0x282c // RX Int. Absolute Delay Timer.
#define REG_RSRPD       0x2c00 // RX Small Packet Detect Interrupt.
#define REG_TIPG        0x0410 // Transmit Inter Packet Gap.
#define ECTRL_SLU       0x40   // Set link up.

#define RCTL_EN            (1 << 1)  // Receiver Enable.
#define RCTL_SBP           (1 << 2)  // Store Bad Packets.
#define RCTL_UPE           (1 << 3)  // Unicast Promiscuous Enabled.
#define RCTL_MPE           (1 << 4)  // Multicast Promiscuous Enabled.
#define RCTL_LPE           (1 << 5)  // Long Packet Reception Enable.
#define RCTL_LBM_NONE      (0 << 6)  // No Loopback.
#define RCTL_LBM_PHY       (3 << 6)  // PHY or external SerDesc loopback.
#define RTCL_RDMTS_HALF    (0 << 8)  // Free Buffer Threshold is 1/2 of RDLEN.
#define RTCL_RDMTS_QUARTER (1 << 8)  // Free Buffer Threshold is 1/4 of RDLEN.
#define RTCL_RDMTS_EIGHTH  (2 << 8)  // Free Buffer Threshold is 1/8 of RDLEN.
#define RCTL_MO_36         (0 << 12) // Multicast Offset - bits 47:36.
#define RCTL_MO_35         (1 << 12) // Multicast Offset - bits 46:35.
#define RCTL_MO_34         (2 << 12) // Multicast Offset - bits 45:34.
#define RCTL_MO_32         (3 << 12) // Multicast Offset - bits 43:32.
#define RCTL_BAM           (1 << 15) // Broadcast Accept Mode.
#define RCTL_VFE           (1 << 18) // VLAN Filter Enable.
#define RCTL_CFIEN         (1 << 19) // Canonical Form Indicator Enable.
#define RCTL_CFI           (1 << 20) // Canonical Form Indicator Bit Value.
#define RCTL_DPF           (1 << 22) // Discard Pause Frames.
#define RCTL_PMCF          (1 << 23) // Pass MAC Control Frames.
#define RCTL_SECRC         (1 << 26) // Strip Ethernet CRC.

// Buffer Sizes
#define RCTL_BSIZE_256   (3 << 16)
#define RCTL_BSIZE_512   (2 << 16)
#define RCTL_BSIZE_1024  (1 << 16)
#define RCTL_BSIZE_2048  (0 << 16)
#define RCTL_BSIZE_4096  ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192  ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))

// Transmit Command
#define CMD_EOP  (1 << 0) // End of Packet.
#define CMD_IFCS (1 << 1) // Insert FCS.
#define CMD_IC   (1 << 2) // Insert Checksum.
#define CMD_RS   (1 << 3) // Report Status.
#define CMD_RPS  (1 << 4) // Report Packet Sent.
#define CMD_VLE  (1 << 6) // VLAN Packet Enable.
#define CMD_IDE  (1 << 7) // Interrupt Delay Enable.

// TCTL Register
#define TCTL_EN         (1 << 1)  // Transmit Enable.
#define TCTL_PSP        (1 << 3)  // Pad Short Packets.
#define TCTL_CT_SHIFT   4         // Collision Threshold.
#define TCTL_COLD_SHIFT 12        // Collision Distance.
#define TCTL_SWXOFF     (1 << 22) // Software XOFF Transmission.
#define TCTL_RTLC       (1 << 24) // Re-transmit on Late Collision.
#define TSTA_DD         (1 << 0)  // Descriptor Done.
#define TSTA_EC         (1 << 1)  // Excess Collisions.
#define TSTA_LC         (1 << 2)  // Late Collision.
#define LSTA_TU         (1 << 3)  // Transmit Underrun.

static const uint16_t i8254x_devices[] = {
    0x1000,    // 82542 (Fiber)
    0x1001,    // 82543GC (Fiber)
    0x1004,    // 82543GC (Copper)
    0x1008,    // 82544EI (Copper)
    0x1009,    // 82544EI (Fiber)
    0x100a,    // 82540EM
    0x100c,    // 82544GC (Copper)
    0x100d,    // 82544GC (LOM)
    0x100e,    // 82540EM
    0x100f,    // 82545EM (Copper)
    0x1010,    // 82546EB (Copper)
    0x1011,    // 82545EM (Fiber)
    0x1012,    // 82546EB (Fiber)
    0x1013,    // 82541EI
    0x1014,    // 82541ER
    0x1015,    // 82540EM (LOM)
    0x1016,    // 82540EP (Mobile)
    0x1017,    // 82540EP
    0x1018,    // 82541EI
    0x1019,    // 82547EI
    0x101a,    // 82547EI (Mobile)
    0x101d,    // 82546EB
    0x101e,    // 82540EP (Mobile)
    0x1026,    // 82545GM
    0x1027,    // 82545GM
    0x1028,    // 82545GM
    0x105b,    // 82546GB (Copper)
    0x1075,    // 82547GI
    0x1076,    // 82541GI
    0x1077,    // 82541GI
    0x1078,    // 82541ER
    0x1079,    // 82546GB
    0x107a,    // 82546GB
    0x107b,    // 82546GB
    0x107c,    // 82541PI
    0x10b5,    // 82546GB (Copper)
    0x1107,    // 82544EI
    0x1112,    // 82544GC
    0xffff
};

int e1000_enabled; // Mark if the driver is enabled.
static struct e1000 e1000; // Hold the driver information.

static void e1000_write_command(uint16_t address, uint32_t value) {
    mmio_write32(e1000.mem_base + address, value);
}

static uint32_t e1000_read_command(uint16_t address) {
    return mmio_read32(e1000.mem_base + address);
}

static void e1000_detect_eeprom(void) {
    uint32_t val = 0;
    e1000_write_command(REG_EEPROM, 0x1);

    for(int i = 0; i < 1000 && !e1000.has_eeprom; i++) {
        val = e1000_read_command(REG_EEPROM);

        if (val & 0x10) {
            e1000.has_eeprom = 1;
        } else {
            e1000.has_eeprom = 0;
        }
    }
}

static uint32_t e1000_eeprom_read(uint8_t address) {
	uint32_t tmp = 0;

    if (e1000.has_eeprom) {
        e1000_write_command(REG_EEPROM, 1 | (address << 8));

        while(!((tmp = e1000_read_command(REG_EEPROM)) & (1 << 4)));
    } else {
        e1000_write_command(REG_EEPROM, 1 | (address << 2));
        while(!((tmp = e1000_read_command(REG_EEPROM)) & (1 << 1)));
    }

	return (tmp >> 16) & 0xFFFF;
}

static void e1000_read_mac(void) {
    if (e1000.has_eeprom) {
        uint32_t temp;
        temp = e1000_eeprom_read(0);
        e1000.mac[0] = temp & 0xff;
        e1000.mac[1] = temp >> 8;
        temp = e1000_eeprom_read(1);
        e1000.mac[2] = temp & 0xff;
        e1000.mac[3] = temp >> 8;
        temp = e1000_eeprom_read(2);
        e1000.mac[4] = temp & 0xff;
        e1000.mac[5] = temp >> 8;
    } else {
        uint8_t *mem_base_mac = (uint8_t *)(e1000.mem_base + 0x5400);

        for(int i = 0; i < 6; i++) {
            e1000.mac[i] = mem_base_mac[i];
        }
    }
}

static void e1000_start_receive(void) {
    e1000.receives = kalloc(sizeof(struct e1000_receive *) * E1000_RECEIVE_COUNT + 16);

    for(int i = 0; i < E1000_RECEIVE_COUNT; i++) {
        e1000.receives[i] = (struct e1000_receive *)((uint8_t *)e1000.receives + i * 16);
        e1000.receives[i]->address = (uint64_t)(kalloc(8192 + 16));
        e1000.receives[i]->status = 0;
    }

    e1000_write_command(REG_TXDESCLO, (uint64_t)e1000.receives >> 32);
    e1000_write_command(REG_TXDESCHI, (uint64_t)e1000.receives & 0xffffffff);

    e1000_write_command(REG_RXDESCLO, (uint64_t)e1000.receives);
    e1000_write_command(REG_RXDESCHI, 0);

    e1000_write_command(REG_RXDESCLEN, E1000_RECEIVE_COUNT * 16);

    e1000_write_command(REG_RXDESCHEAD, 0);
    e1000_write_command(REG_RXDESCTAIL, E1000_RECEIVE_COUNT - 1);
    e1000.current_receive = 0;

    e1000_write_command(REG_RCTRL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE
        | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_8192);
}

static void e1000_start_transmit(void) {
    e1000.transmits = kalloc(sizeof(struct e1000_transmit *) * E1000_TRANSMIT_COUNT + 16);

    for (int i = 0; i < E1000_TRANSMIT_COUNT; i++) {
        e1000.transmits[i] = (struct e1000_transmit *)((uint8_t *)e1000.transmits + i * 16);
        e1000.transmits[i]->address = 0;
        e1000.transmits[i]->cmd = 0;
        e1000.transmits[i]->status = TSTA_DD;
    }

    e1000_write_command(REG_TXDESCHI, (uint64_t)(e1000.transmits) >> 32);
    e1000_write_command(REG_TXDESCLO, (uint64_t)(e1000.transmits) & 0xffffffff);

    // Now setup total length of descriptors
    e1000_write_command(REG_TXDESCLEN, E1000_TRANSMIT_COUNT * 16);

    // Setup numbers.
    e1000_write_command(REG_TXDESCHEAD, 0);
    e1000_write_command(REG_TXDESCTAIL, 0);
    e1000.current_transmit = 0;

    e1000_write_command(REG_TCTRL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT)
        | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);

    // This line of code overrides the one before it but I left both to highlight
    // that the previous one works with e1000 cards, but for the e1000e cards
    // you should set the TCTRL register as follows. For detailed description of
    // each bit, please refer to the Intel Manual.
    // In the case of I217 and 82577LM packets will not be sent if the TCTRL is
    // not configured using the following bits.
    e1000_write_command(REG_TCTRL, 0b0110000000000111111000011111010);
    e1000_write_command(REG_TIPG, 0x0060200a);
}

void init_e1000(void) {
    e1000_enabled = 0;
    kprint(KPRN_INFO, "e1000: Initialising E1000 device driver...");

    // First, get the PCI device, searching for all the models.
    struct pci_device_t *device = NULL;

    for (int i = 0; i8254x_devices[i] != 0xffff && !device; i++) {
        device = pci_get_device_by_vendor(INTEL_VEND, i8254x_devices[i]);
    }

    if (!device) {
        kprint(KPRN_INFO, "e1000: Could not find pci device, aborted!");
        return;
    }

    // Enable bus mastering for this device.
    pci_enable_busmastering(device);

    // Find IO addresses and other useful info.
    
    struct pci_bar_t bar = {0};
    panic_if(pci_read_bar(device, 0, &bar));
    panic_unless(bar.is_mmio);

    e1000.mem_base = bar.base + MEM_PHYS_OFFSET;
    e1000_detect_eeprom();
    e1000_read_mac();
    kprint(KPRN_INFO, "e1000: MAC address: %x:%x:%x:%x:%x:%x", e1000.mac[0],
        e1000.mac[1], e1000.mac[2], e1000.mac[3], e1000.mac[4], e1000.mac[5]);

    for(int i = 0; i < 0x80; i++) {
        e1000_write_command(0x5200 + i * 4, 0);
    }

    // Start the assigned interrupt and transmit.
    e1000_start_receive();
    e1000_start_transmit();
    kprint(KPRN_INFO, "e1000: Card started");

    // We got it! Set the device as enabled and return.
    e1000_enabled = 1;
}
