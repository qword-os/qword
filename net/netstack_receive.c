#include <sys/panic.h>
#include <lib/dynarray.h>
#include <net/proto/ether.h>
#include <net/proto/arp.h>
#include <lib/errno.h>
#include <lib/cmem.h>
#include <net/proto/ipv4.h>
#include <net/proto/udp.h>
#include "netstack.h"
#include "net.h"
#include "socket.h"

// TODO: we could easily remove the length from the packet structure on
//       each OSI level

////////////////////////////////////////////////////////////////////////
// Transport layer processing
////////////////////////////////////////////////////////////////////////

static int process_tcp(struct packet_t* pkt) {
    kprint(KPRN_ERR, "netstack: tcp: TODO");
    return -1;
}


static int process_udp(struct packet_t* pkt) {
    if (sizeof(struct udp_hdr_t) > pkt->data_len) {
        kprint(KPRN_ERR, "netstack: udp: invalid length");
        return -1;
    }

    // get the header and covert endianess
    struct udp_hdr_t* udp = (struct udp_hdr_t *) pkt->data;
    udp->length = NTOHS(udp->length);

    pkt->transport.src = NTOHS(udp->src_port);
    pkt->transport.dst = NTOHS(udp->dst_port);
    pkt->transport.size = sizeof(struct udp_hdr_t);

    // make sure the length is fine
    if (udp->length != pkt->data_len) {
        kprint(KPRN_ERR, "netstack: udp: invalid packet length");
        return -1;
    }

    pkt->data += sizeof(struct udp_hdr_t);
    pkt->data_len -= sizeof(struct udp_hdr_t);

    // verify checksum
    if (pkt->nic->flags & NIC_RX_UDP_CS) {
        if (!(pkt->nic_flags & PKT_FLAG_UDP_CS)) {
            kprint(KPRN_ERR, "netstack: udp: got invalid checksum (hardware offload)");
            return -1;
        }
    } else {
        kprint(KPRN_ERR, "netstack: udp: TODO software checksum calculation");
        return -1;
    }

    // TODO: pass to the higher level stack for routing
    //       to the correct socket

    kprint(KPRN_DBG, "netstack: udp: got udp request at port %d", pkt->transport.dst);
    return 0;
}

static int process_transport(struct packet_t* pkt) {
    switch (pkt->network.type) {
        case PROTO_TCP: return process_tcp(pkt);
        case PROTO_UDP: return process_udp(pkt);
        default:
            kprint(KPRN_ERR, "netstack: transport: unknown protocol %x", pkt->network.type);
            return -1;
    }
}

////////////////////////////////////////////////////////////////////////
// Network layer processing
////////////////////////////////////////////////////////////////////////

static int process_arp(struct packet_t* pkt) {
    if (sizeof(struct arp_hdr_t) > pkt->data_len) {
        kprint(KPRN_ERR, "netstack: arp: invalid length");
        return -1;
    }

    struct arp_hdr_t* arp = (struct arp_hdr_t *) pkt->data;
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
        struct arp_ipv4_t* ipv4_arp = (struct arp_ipv4_t *) pkt->data;
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

static int process_ipv4(struct packet_t* pkt) {
    if (pkt->data_len < sizeof(struct ipv4_hdr_t)) {
        kprint(KPRN_ERR, "netstack: ipv4: invalid length");
        return -1;
    }

    // get the header and convert needed data
    struct ipv4_hdr_t* header = (struct ipv4_hdr_t *) pkt->data;
    header->total_len = NTOHS(header->total_len);
    header->fragment = NTOHS(header->fragment);

    // the size must match
    if (header->total_len != pkt->data_len) {
        kprint(KPRN_ERR, "netstack: ipv4: invalid length");
        return -1;
    }

    // check the header is valid
    if (header->head_len * 4 > header->total_len) {
        kprint(KPRN_ERR, "netstack: ipv4: invalid header length");
        return -1;
    }

    pkt->network.size = header->head_len * 4;
    pkt->network.type = header->protocol;
    pkt->network.src = header->src;
    pkt->network.dst = header->dst;

    // TODO: process options

    pkt->data += header->head_len * 4;
    pkt->data_len -= header->head_len * 4;

    // make sure the version is good
    if (header->ver != 4) {
        kprint(KPRN_ERR, "netstack: ipv4: got ipv4 packet with invalid version");
        return -1;
    }

    // TODO: fragmentation
    if (header->fragment & IPV4_HEAD_MF_MASK || (header->fragment & IPV4_HEAD_OFFSET_MASK) != 0) {
        kprint(KPRN_ERR, "netstack: ipv4: dropped a fragmented packet");
        return -1;
    }

    // check if this is for us
    if (!IPV4_EQUAL(pkt->nic->ipv4_addr, pkt->network.dst)) {
        kprint(KPRN_DBG, "netstack: ipv4: got packet not for us (%d.%d.%d.%d), ignoring",
                pkt->network.dst.addr[0],
                pkt->network.dst.addr[1],
                pkt->network.dst.addr[2],
                pkt->network.dst.addr[3]);
        return -1;
    }

    // verify checksum
    if (pkt->nic->flags & NIC_RX_IP_CS) {
        if (!(pkt->nic_flags & PKT_FLAG_IP_CS)) {
            kprint(KPRN_ERR, "netstack: ipv4: got invalid checksum (hardware offload)");
            return -1;
        }
    } else {
        kprint(KPRN_ERR, "netstack: ipv4: TODO software checksum calculation");
        return -1;
    }

    // pass to transport
    kprint(KPRN_DBG, "GOT IPV4 PACKET %d.%d.%d.%d",
           pkt->network.dst.addr[0],
           pkt->network.dst.addr[1],
           pkt->network.dst.addr[2],
           pkt->network.dst.addr[3]);
    return process_transport(pkt);
}

static int process_network(struct packet_t* pkt) {
    switch (pkt->datalink.type) {
        case ETHER_ARP:     return process_arp(pkt);
        case ETHER_IPV4:    return process_ipv4(pkt);
        default:
            kprint(KPRN_ERR, "netstack: network: unknown protocol %x", pkt->datalink.type);
            return -1;
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
    struct ether_hdr* hdr = (struct ether_hdr *) pkt->data;
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

void netstack_process_frame(struct packet_t* pkt) {

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
        buffer->buf = kalloc(pkt->data_len);
        buffer->len = pkt->data_len;
        memcpy(buffer->buf, pkt->data, pkt->data_len);

        // link it
        sock->last_buffer->next = buffer;
        sock->last_buffer = buffer;

        dynarray_unref(sockets, i);
        spinlock_release(&sock->buffers_lock);
    }

    // just process it
    process_datalink(pkt);
}
