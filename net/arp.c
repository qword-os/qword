#include <net/arp.h>
#include <lib/klib.h>
#include <lib/cio.h>
#include <lib/endian.h>

#define ETHERNET_HW_TYPE 0x0001
#define IPv4_PROTO_TYPE  0x0800

#define REQUEST_OPERATION_TYPE 1
#define REPLY_OPERATION_TYPE   2

void arp_handle_packet(struct arp_header *packet, size_t length) {
    kprint(KPRN_INFO, "arp: Handling packet of %d bytes", length);

    // Check that we are dealing with IPv4 and ethernet.
    int hardware_type = bswap16(packet->hardware_type);
    int protocol_type = bswap16(packet->protocol_type);

    if (hardware_type != ETHERNET_HW_TYPE || protocol_type != IPv4_PROTO_TYPE) {
        kprint(KPRN_WARN, "arp: Non Ethernet & IPv4 packet ignored");
        return;
    }

    // Check operation type and act accordingly.
    int operation = bswap16(packet->operation);

    switch (operation) {
        case REQUEST_OPERATION_TYPE:
            break;
        case REPLY_OPERATION_TYPE:
            break;
        default:
            kprint(KPRN_WARN, "arp: Non recognised operation");
    }
}
