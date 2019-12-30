#include <sys/pci.h>
#include <sys/idt.h>
#include <sys/apic.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <mm/mm.h>
#include <proc/task.h>

// PCI vendor and ID.
#define RTL8139_VENDOR 0x10ec
#define RTL8139_DEV_ID 0x8139

// Registers.
#define MAC_05_REG           0x00
#define MAC_07_REG           0X04
#define RECEIVE_BUFFER_START 0x30 // Didnt find a name so gave one of my own.
#define COMMAND_REG          0x37
#define CAPR                 0x38 // CAPR - isun xdddd
#define RCR_REG              0x44
#define CONFIG_1_REG         0x52
#define IMR_REG              0x3c
#define REQUEST_REG          0x3e // Ditto.

// Size of receive.
#define RECEIVE_SIZE (8192 + 16 + 1500)

// Important values.
#define ROK (1 << 0)
#define TOK (1 << 2)
#define RECEIVE_READ_POINTER_MASK (~3)

// PCI device for the card.
static struct pci_device_t *device = NULL;

// Internal state of the card.
static int io_address;
static int irq_line;
static uint8_t rtl8139_mac[6];
static size_t current_packet;
static uint8_t *receive_buffer;

static void rtl8139_receive_packet(void) {
    uint16_t *t = (uint16_t *)(receive_buffer + MEM_PHYS_OFFSET + current_packet);
    // Skip packet header, get packet length
    uint16_t packet_length = *(t + 1);

    // Skip, packet header and packet length, now t points to the packet data
    t = t + 2;

    // Now, ethernet layer starts to handle the packet(be sure to make a copy of the packet, insteading of using the buffer)
    // and probabbly this should be done in a separate thread...
    // void *packet = kmalloc(packet_length);
    // memcpy(packet, t, packet_length);
    // ethernet_handle_packet(packet, packet_length);

    current_packet = (current_packet + packet_length + 4 + 3) & RECEIVE_READ_POINTER_MASK;

    if (current_packet > RECEIVE_SIZE) {
        current_packet -= RECEIVE_SIZE;
    }

    port_out_w(io_address + CAPR, current_packet - 0x10);
}

static void rtl8139_handler(void) {
    for (;;) {
        event_await(&int_event[irq_line]);
        uint16_t status = port_in_w(io_address + REQUEST_REG);

        if (status & TOK) {
            kprint(KPRN_INFO, "rtl8139: Package sent");
        }

        if (status & ROK) {
            kprint(KPRN_INFO, "rtl8139: Request received");
            rtl8139_receive_packet();
        }

        port_out_d(io_address + REQUEST_REG, 0x5);
    }
}

static void rtl8139_read_mac(void) {
    uint32_t mac_part1 = port_in_d(io_address + MAC_05_REG);
    uint16_t mac_part2 = port_in_w(io_address + MAC_07_REG);

    rtl8139_mac[0] = mac_part1 >> 0;
    rtl8139_mac[1] = mac_part1 >> 8;
    rtl8139_mac[2] = mac_part1 >> 16;
    rtl8139_mac[3] = mac_part1 >> 24;
    rtl8139_mac[4] = mac_part2 >> 0;
    rtl8139_mac[5] = mac_part2 >> 8;
}

int probe_rtl8139(void) {
    device = pci_get_device_by_vendor(RTL8139_VENDOR, RTL8139_DEV_ID, 0);
    return device ? 0 : -1;
}

void init_rtl8139(void) {
    kprint(KPRN_INFO, "rtl8139: Initialising device driver...");

    // Get information required for accessing the card and busmaster.
    io_address = pci_read_device_dword(device, 0x10) & ~0x3;
    irq_line = get_empty_int_vector();
    pci_enable_busmastering(device);
    kprint(KPRN_INFO, "rtl8139: Addressing in IO %x and IRQ line %x", io_address, irq_line);

    // Turn on the card by setting the LWAKE + LWPTN bits.
    port_out_b(io_address + CONFIG_1_REG, 0x0);

    // Set everything to default by resetting the card.
    port_out_b(io_address + COMMAND_REG, 0x10);
    while ((port_in_b(io_address + COMMAND_REG) & 0x10) != 0);

    // Set the receive buffer start for the card.
    receive_buffer = pmm_alloc(DIV_ROUNDUP(RECEIVE_SIZE, PAGE_SIZE));
    port_out_d(io_address + RECEIVE_BUFFER_START, (size_t)receive_buffer);

    // Set the transmission ok (TOK) and receive ok (ROK) irqs.
    port_out_w(io_address + IMR_REG, 0x5);

    // Configure the receive buffer.
    // Set it to AB, AM, APM and AAP, also set the WRAP bit.
    port_out_d(io_address + RCR_REG, 0xf | (1 << 7));

    // Enable receiving and trasmitting with the RE and TE bits.
    port_out_b(io_address + COMMAND_REG, 0x0c);

    // Read MAC.
    rtl8139_read_mac();
    kprint(KPRN_INFO, "rtl8139: MAC=%x:%x:%x:%x:%x:%x", rtl8139_mac[0],
        rtl8139_mac[1], rtl8139_mac[2], rtl8139_mac[3], rtl8139_mac[4],
        rtl8139_mac[5]);

    // Register the interrupt with its handler.
    io_apic_connect_gsi_to_vec(0, irq_line, device->gsi, device->gsi_flags, 1);
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(0, rtl8139_handler, 0));
}
