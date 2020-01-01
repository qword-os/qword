#include <net/ethernet.h>
#include <net/nic.h>
#include <net/mac.h>
#include <net/arp.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <lib/endian.h>

#define HEADER_SIZE sizeof(struct ethernet_header)
#define FOOTER_SIZE sizeof(uint32_t)

#define ARP_ETHERNET_TYPE 0x806

static const mac_t BROADCAST = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_handle_packet(struct ethernet_header *packet, size_t length) {
    kprint(KPRN_INFO, "ethernet: Handling packet of %d bytes", length);
    print_mac("ethernet: Destination mac", packet->destination_mac);
    print_mac("ethernet: Source mac", packet->source_mac);

    int ethernet_type = bswap16(packet->ethernet_type);
    kprint(KPRN_INFO, "ethernet: Ethernet type %x", ethernet_type);

    if (are_macs_equal(packet->destination_mac, BROADCAST) || are_macs_equal(packet->destination_mac, nic_mac)) {
        switch (ethernet_type) {
            case ARP_ETHERNET_TYPE:
                arp_handle_packet((uint8_t *)packet + HEADER_SIZE - 1, length - HEADER_SIZE - FOOTER_SIZE);
                break;
            default:
                kprint(KPRN_INFO, "ethernet: Unrecognised ethernet type, ignored");
        }
    } else {
        kprint(KPRN_INFO, "ethernet: Packet ignored");
    }
}
