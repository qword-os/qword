#include <fs/devfs/devfs.h>
#include <lib/dynarray.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <sys/panic.h>
#include <sys/apic.h>
#include <sys/idt.h>
#include <sys/pci.h>
#include <stdint.h>
#include <stddef.h>
#include <acpi/lai/core/libc.h>
#include <net/netstack.h>
#include <stdbool.h>

#include "rtl81x9_private.h"

#define RX_BUFFER_SIZE  (1536u)
#define RX_RING_SIZE    (64u)
#define TX_RING_SIZE    (64u)

struct r81x9_device_t {
    // devfs
    dev_t dev;

    // nic
    struct nic_t nic;

    // device info
    uintptr_t base;
    int irq_line;
    int poll_reg;
    unsigned ipf;
    unsigned udpf;
    unsigned tcpf;

    // rings
    volatile struct pkt_desc_t* rx_ring;
    volatile struct pkt_desc_t* tx_ring;

    // index in ring
    size_t rxi;

    // Tx related stuff
    size_t txi;
    size_t prev_txi;
    lock_t tx_lock;
};

// list of supported rtl cards
static uint16_t supported_devices[] = {
        0x8139, // only cards with revision >= 0x20
        0x8168,
        0x8169
};

dynarray_new(struct r81x9_device_t, devices);

/**
 * This is called on ROK interrupt. It will go through the
 */
static void process_received_packets(struct r81x9_device_t* dev) {
    for (size_t left = RX_RING_SIZE; left > 0; left--, dev->rxi = (dev->rxi + 1) % RX_RING_SIZE) {
        volatile struct pkt_desc_t* desc = &dev->rx_ring[dev->rxi];

        // make sure the nic does not own the desc
        if (desc->opts1 & OWN) {
            break;
        }

        // make sure we do not touch descriptor
        // which is not ours
        memory_barrier();

        // TODO: support packet split
        if (!(desc->opts1 & (LS | FS))) {
            kprint(KPRN_WARN, "rtl81x9: split packets are not supported currently");
            continue;
        }

        // copy the buffer and packet
        uint16_t len = (desc->opts1 & FRAME_LENGTH_MASK) - 4; // ignore the fcs
        uintptr_t buf = desc->buff + MEM_PHYS_OFFSET;

        // get packet flags
        uint64_t flags = 0;
        if (!(desc->opts1 & dev->ipf)) {
            flags |= PKT_FLAG_IP_CS;
        }
        if (!(desc->opts1 & dev->udpf)) {
            flags |= PKT_FLAG_UDP_CS;
        }
        if (!(desc->opts1 & dev->tcpf)) {
            flags |= PKT_FLAG_TCP_CS;
        }

        struct packet_t pkt = {
            .nic = &dev->nic,
            .data = (char *) buf,
            .data_len = len,
            .nic_flags = flags
        };

        // let the network stack handle it
        netstack_process_frame(&pkt);

        // reset the descriptor
        uint16_t eor = desc->opts1 * EOR;
        desc->opts2 = 0;
        desc->opts1 = (eor | OWN | RX_BUFFER_SIZE);
    }
}

/*
 * Will be called on TOK, will just go through the sent packets and tell the netstack we finished
 * using the buffer.
 */
static void process_sent_packets(struct r81x9_device_t* dev) {
    spinlock_acquire(&dev->tx_lock);

    // iterate over the sent packets
    for (; dev->prev_txi != dev->txi; dev->prev_txi = (dev->prev_txi + 1) % TX_RING_SIZE) {
        volatile struct pkt_desc_t* desc = &dev->tx_ring[dev->prev_txi];

        // only touch it if it was sent, we can assume
        // the following ones were not sent as well
        if (desc->opts1 & OWN) {
            break;
        }

        // make sure we do not touch descriptor
        // which is not ours
        memory_barrier();

        // simply free the buffer
        kfree((void *) (desc->buff + MEM_PHYS_OFFSET));

        desc->opts1 &= EOR;
        desc->opts2 = 0;
        desc->buff = 0;
    }

    spinlock_release(&dev->tx_lock);
}

static void rtl81x9_int_handler(struct r81x9_device_t* dev) {
    panic_unless(dev);

    for (;;) {
        // wait for an interrupt and get the event
        event_await(&int_event[dev->irq_line]);
        uint16_t isr = mmio_read16(dev->base + ISR);

        // got a packet
        if (isr & ROK) {
            process_received_packets(dev);
        }

        // sent packets
        if (isr & TOK) {
            process_sent_packets(dev);
        }

        // will help us with catching errors
        if (isr & RER) kprint(KPRN_ERR, "rtl81x9: Rx Error");
        if (isr & TER) kprint(KPRN_ERR, "rtl81x9: Tx Error");
        if (isr & RDU) kprint(KPRN_ERR, "rtl81x9: Rx Descriptor Unavailable");
        if (isr & TDU) kprint(KPRN_ERR, "rtl81x9: Tx Descriptor Unavailable");
        if (isr & SERR) kprint(KPRN_ERR, "rtl81x9: System error");

        // clear status bits
        mmio_write16(dev->base + ISR, isr);
    }
}

// TODO: maybe a write should be a raw write?
static int send_packet(int internal_fd, void* pkt, size_t len, uint64_t flags) {
    panic_unless(len == (len & FRAME_LENGTH_MASK));

    // get the card
    struct r81x9_device_t* dev = dynarray_getelem(struct r81x9_device_t, devices, internal_fd);
    panic_unless(dev);
    spinlock_acquire(&dev->tx_lock);

    // check if we have a free descriptor
    volatile struct pkt_desc_t* desc = &dev->tx_ring[dev->txi];
    if (desc->opts1 & OWN) {
        // no space for more packets try and trigger the
        // nic to send some packets
        mmio_write8(dev->poll_reg, NPQ);

        // don't forget to unlock the device tx
        spinlock_release(&dev->tx_lock);
        dynarray_unref(devices, internal_fd);

        // try again later
        errno = EAGAIN;
        return -1;
    }

    // modify the descriptor
    desc->opts1 |= (len | OWN | LS | FS);
    desc->opts2 = 0;
    desc->buff = (uintptr_t)pkt - MEM_PHYS_OFFSET;

    // set flags
    if (flags & PKT_FLAG_IP_CS) {
        desc->opts1 |= IPCS;
    }
    if (flags & PKT_FLAG_UDP_CS) {
        desc->opts1 |= UDPCS;
    }
    if (flags & PKT_FLAG_TCP_CS) {
        desc->opts1 |= TCPCS;
    }

    // increment to the next packet
    dev->txi++;

    // tell the nic to send it
    // TODO: instead of nqp on every packet maybe have a locked count so we know how
    //       many more want to send
    memory_barrier();
    mmio_write8(dev->base + dev->poll_reg, NPQ);

    // don't forget to unlock the device tx
    spinlock_release(&dev->tx_lock);
    dynarray_unref(devices, internal_fd);
    return 0;
}

static void init_rtl81x9_dev(struct pci_device_t* device) {
    struct r81x9_device_t nic = {0};
    struct pci_bar_t bar = {0};

    // get the bar
    bool found = false;
    for (int i = 0; i < 6; i++) {
        if (!pci_read_bar(device, i, &bar) && bar.is_mmio) {
            found = true;
            break;
        }
    }

    // make sure we actually found a good device
    // if so set the correct base
    if (!found) {
        kprint(KPRN_WARN, "rtl81x9: could not find mmio bar, ignoring device");
        return;
    }
    nic.base = bar.base + MEM_PHYS_OFFSET;

    // enable bus mastering and interrupts
    pci_enable_busmastering(device);
    pci_enable_interrupts(device);

    // reset the device
    mmio_write8(nic.base + CR, RST);
    int timeout = 0;
    while (mmio_read8(nic.base + CR) & RST) {
        if (timeout++ == 100000) {
            kprint(KPRN_WARN, "rtl81x9: reset timeout, skipping device");
            return;
        }
    }

    // allocate the rx & tx rings
    // TODO: maybe we should use a single allocation
    nic.rx_ring = kalloc(sizeof(struct pkt_desc_t) * RX_RING_SIZE);
    nic.tx_ring = kalloc(sizeof(struct pkt_desc_t) * TX_RING_SIZE);
    panic_unless(nic.rx_ring && nic.tx_ring);

    // allocate a bunch of tx buffers
    uintptr_t pkt_buffs_base = (uintptr_t) pmm_alloc((RX_RING_SIZE * RX_BUFFER_SIZE) / PAGE_SIZE);
    panic_unless(pkt_buffs_base);

    // setup the rx descriptors
    for (size_t i = 0; i < RX_RING_SIZE; i++, pkt_buffs_base += RX_BUFFER_SIZE) {
        // setup the rx entry
        volatile struct pkt_desc_t* desc = &nic.rx_ring[i];
        desc->opts1 = (OWN | RX_BUFFER_SIZE);
        desc->opts2 = 0;
        desc->buff = pkt_buffs_base;
    }

    // set the end of ring bit for the tx & rx
    nic.rx_ring[RX_RING_SIZE - 1].opts1 |= EOR;
    nic.tx_ring[TX_RING_SIZE - 1].opts1 |= EOR;

    /*
     * Accept broadcast and physically match packets
     * Unlimited DMA burst
     * No rx threshold
     */
    mmio_write32(nic.base + RCR, APM | AB | MXDMA_UNLIMITED | RXFTH_NONE);

    /**
     * append crc to every frame
     * Unlimited DMA burst
     * normal IFG
     */
    mmio_write8(nic.base + CR, TE);
    mmio_write32(nic.base + TCR, MXDMA_UNLIMITED | CRC | IFG_NORMAL);

    // setup rx checksum checking
    // on 8139 we need to enable C+ mode on the RX and TX rings
    // also on the 8139 the poll register is at a different offset
    if (device->device_id == 0x8139) {
        nic.poll_reg = TPPoll_8139;
        nic.ipf = IPF_RTL8139;
        nic.udpf = UDPF_RTL8139;
        nic.tcpf = TCPF_RTL8139;
        mmio_write16(nic.base + CPCR, RxChkSum | CPRx | CPTx);
    } else {
        nic.poll_reg = TPPoll;
        nic.ipf = IPF;
        nic.udpf = UDPF;
        nic.tcpf = TCPF;
        mmio_write16(nic.base + CPCR, RxChkSum);
    }

    // setup the descriptors
    uintptr_t rx_ring_phys = (uintptr_t)nic.rx_ring - MEM_PHYS_OFFSET;
    uintptr_t tx_ring_phys = (uintptr_t)nic.tx_ring - MEM_PHYS_OFFSET;
    mmio_write32(nic.base + RDSAR_LOW, rx_ring_phys & 0xFFFFFFFF);
    mmio_write32(nic.base + RDSAR_HIGH, (rx_ring_phys >> 32u) & 0xFFFFFFFF);
    mmio_write32(nic.base + TNPDS_LOW, tx_ring_phys & 0xFFFFFFFF);
    mmio_write32(nic.base + TNPDS_HIGH, (tx_ring_phys >> 32u) & 0xFFFFFFFF);

    // add the card to the list of devices
    int i = dynarray_add(struct r81x9_device_t, devices, &nic);
    struct r81x9_device_t* dev = dynarray_getelem(struct r81x9_device_t, devices, i);
    dynarray_unref(devices, i);

    // make sure the lock is not locked
    dev->tx_lock = new_lock;

    // add to /dev
    struct device_t devfs_device = {0};
    lai_snprintf(devfs_device.name, sizeof(devfs_device), "eth%02x%x%x", device->bus, device->device, device->func);
    devfs_device.size = 0;
    devfs_device.intern_fd = i;
    devfs_device.calls = default_device_calls;
    dev->dev = device_add(&devfs_device);

    // create nic and add to network stack
    dev->nic.internal_fd = i;
    dev->nic.flags = NIC_RX_IP_CS | NIC_RX_UDP_CS |NIC_RX_TCP_CS | NIC_TX_IP_CS | NIC_TX_UDP_CS | NIC_TX_TCP_CS;
    dev->nic.calls.send_packet = send_packet;
    dev->nic.mac_addr.raw[0] = mmio_read8(dev->base + IDR0);
    dev->nic.mac_addr.raw[1] = mmio_read8(dev->base + IDR1);
    dev->nic.mac_addr.raw[2] = mmio_read8(dev->base + IDR2);
    dev->nic.mac_addr.raw[3] = mmio_read8(dev->base + IDR3);
    dev->nic.mac_addr.raw[4] = mmio_read8(dev->base + IDR4);
    dev->nic.mac_addr.raw[5] = mmio_read8(dev->base + IDR5);
    net_add_nic(&dev->nic);

    // set interrupt handler
    dev->irq_line = get_empty_int_vector();
    panic_if(dev->irq_line < 0);
    if (!pci_register_msi(device, dev->irq_line)) {
        panic_if(device->gsi == UINT32_MAX);
        io_apic_connect_gsi_to_vec(0, dev->irq_line, device->gsi, device->gsi_flags, 1);
    }
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(0, rtl81x9_int_handler, dev));

    // enable Rx and Tx rings
    mmio_write8(nic.base + CR, RE | TE);

    // enable interrupts
    mmio_write16(nic.base + IMR, ROK | TOK | TER | RER | SERR | RDU | TDU);
}

void probe_rtl81x9() {
    kprint(KPRN_INFO, "rtl81x9: probing realtek devices");

    struct pci_device_t* cur_dev;
    for (size_t i = 0; i < sizeof(supported_devices) / sizeof(uint16_t); i++) {
        size_t j = 0;
        while ((cur_dev = pci_get_device_by_vendor(RTL_VENDOR_ID, supported_devices[i], j++))) {

            // make sure the device supports C+ mode
            // TODO: for non-C+ mode devices we need a separate driver
            if (cur_dev->device_id == 0x8139 && cur_dev->rev_id < 0x20) {
                continue;
            }

            kprint(KPRN_INFO, "rtl81x9: found %x at %x:%x.%x", cur_dev->device_id, cur_dev->bus, cur_dev->device, cur_dev->func);
            init_rtl81x9_dev(cur_dev);
        }
    }
}
