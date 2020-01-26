#include <sys/panic.h>
#include <lib/dynarray.h>
#include <net/proto/ether.h>
#include <net/proto/arp.h>
#include <lib/errno.h>
#include <lib/cmem.h>
#include "netstack.h"
#include "net.h"
#include "socket.h"

////////////////////////////////////////////////////////////////////////
// Network layer processing
////////////////////////////////////////////////////////////////////////

static int process_arp(struct packet_t* pkt) {
    if (sizeof(struct arp_hdr_t) > pkt->data_len) {
        kprint(KPRN_ERR, "netstack: arp: invalid length");
        return -1;
    }

    struct arp_hdr_t* arp = pkt->data;
    pkt->data += sizeof(struct arp_hdr_t);
    pkt->data_len -= sizeof(struct arp_hdr_t);

    // make sure the hardware is ethernet
    if (arp->hw_type != ARP_HW_ETHER || arp->hw_addr_len != sizeof(mac_addr_t)) {
        kprint(KPRN_ERR, "netstack: arp: expected ethernet, got %x (hw len %d)", arp->hw_type, arp->hw_addr_len);
        return -1;
    }

    // NOTE: in theory we can abstract this to work on any protocol length
    //       but fuck that, arp is only used for IPv4 anyways
    if (arp->proto_type == ARP_PROTO_IPV4 && arp->proto_addr_len == sizeof(ipv4_addr_t)) {
        // validate length
        if (sizeof(struct arp_ipv4_t) > pkt->data_len) {
            kprint(KPRN_ERR, "netstack: arp/ipv4: invalid length");
            return -1;
        }

        // check for valid opcode
        if (arp->opcode != ARP_OPCODE_REQUEST && arp->opcode != ARP_OPCODE_REPLY) {
            kprint(KPRN_ERR, "netstack: arp/ipv4: invalid arp request");
            return -1;
        }

        // reply if this is for us
        struct arp_ipv4_t* ipv4_arp =  pkt->data;
        pkt->network.dst = ipv4_arp->target_ip;
        pkt->network.src = ipv4_arp->sender_ip;
        pkt->datalink.dst = ipv4_arp->target_mac;
        pkt->datalink.src = ipv4_arp->sender_mac;
        pkt->network.type = arp->opcode;

        // the arp server does not need the data
        pkt->data = NULL;
        pkt->data_len = 0;

        // pass to the arp handling
        arp_process_packet(pkt);

        return 0;
    } else {
        // invalid
        kprint(KPRN_ERR, "netstack: arp: unknown protocol type %x (proto len %d)", arp->proto_type, arp->proto_addr_len);
        return -1;
    }
}

static int process_network(struct packet_t* pkt) {
    switch (pkt->datalink.type) {
        case ETHER_ARP: return process_arp(pkt);
        // TODO: ipv4
        default:        return -1;
    }
}

////////////////////////////////////////////////////////////////////////
// Datalink layer processing
////////////////////////////////////////////////////////////////////////

static int process_datalink(struct packet_t* pkt) {
    // drop packet if too small
    if (pkt->data_len < sizeof(struct ether_hdr)) {
        kprint(KPRN_ERR, "netstack: ether: invalid length");
        return -1;
    }

    // NOTE: this assumes ethernet
    // copy the info to the packet
    struct ether_hdr* hdr = pkt->data;
    pkt->datalink.dst = hdr->dst;
    pkt->datalink.src = hdr->src;
    pkt->datalink.type = hdr->type;
    pkt->datalink.size = sizeof(struct ether_hdr);

    // process the next layer
    pkt->data += sizeof(struct ether_hdr);
    pkt->data_len -= sizeof(struct ether_hdr);
    return process_network(pkt);
}

////////////////////////////////////////////////////////////////////////
// Network stack
////////////////////////////////////////////////////////////////////////

void netstack_process_frame(struct nic_t* nic, void* packet, size_t len) {
    struct packet_t pkt = {
        .data_len = len,
        .data = packet,
        .nic = nic
    };

    // process any packet sockets
    // TODO: this could really benefit from having a separate list
    //       only for packet sockets
    size_t i = 0;
    struct socket_t* sock = NULL;
    while (1) {
        sock = dynarray_search(struct socket_t, sockets, &i, elem->domain == AF_PACKET, i);
        if (sock == NULL) {
            break;
        }

        spinlock_acquire(&sock->buffers_lock);

        // copy the packet
        struct socket_buffer_t* buffer = kalloc(sizeof(struct socket_buffer_t));
        buffer->buf = kalloc(len);
        buffer->len = len;
        memcpy(buffer->buf, packet, len);

        // link it
        sock->last_buffer->next = buffer;
        sock->last_buffer = buffer;

        dynarray_unref(sockets, i);
        spinlock_release(&sock->buffers_lock);
    }

    if (process_datalink(&pkt) < 0) {
        kprint(KPRN_WARN, "netstack: dropped packet");
    }
}
