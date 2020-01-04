#include <net/ethernet.h>
#include <net/nic.h>
#include <net/arp.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <lib/endian.h>

#define HEADER_SIZE (sizeof(struct ethernet_header))

#define ARP_ETHERNET_TYPE 0x806

static const mac_t BROADCAST = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_handle_packet(mac_t nic_mac, struct ethernet_header *packet, size_t length) {
    kprint(KPRN_INFO, "ethernet: Handling packet of %d bytes", length);

    // Bitswap ethernet type.
    int ethernet_type = bswap16(packet->ethernet_type);

    // Check MACs and ethernet types.
    if (are_macs_equal(packet->destination_mac, BROADCAST) || are_macs_equal(packet->destination_mac, nic_mac)) {
        switch (ethernet_type) {
            case ARP_ETHERNET_TYPE:
                arp_handle_packet((uint8_t *)packet + HEADER_SIZE, length - HEADER_SIZE);
                break;
            default:
                kprint(KPRN_WARN, "ethernet: Unrecognised ethernet type, ignored");
        }
    } else {
        kprint(KPRN_WARN, "ethernet: Packet ignored");
    }
}
