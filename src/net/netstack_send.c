
#include <lib/klib.h>
#include <net/proto/ether.h>
#include <lib/cmem.h>
#include <lib/errno.h>
#include <net/proto/ipv4.h>
#include <net/proto/udp.h>
#include <net/net.h>
#include <net/netstack.h>
#include <fd/socket/socket.h>

static int process_transport(struct packet_t* pkt, char* buf, size_t len) {
    switch (pkt->network.type) {
        // handle udp
        case PROTO_UDP: {
            if (len < sizeof(struct udp_hdr_t)) {
                kprint(KPRN_WARN, "netstack: udp: not enough space in frame to send packet");
                errno = ENOBUFS;
                return -1;
            }

            // construct header
            struct udp_hdr_t* hdr = (struct udp_hdr_t *) buf;
            hdr->dst_port = HTONS(pkt->transport.dst);
            hdr->src_port = HTONS(pkt->transport.src);
            hdr->length = HTONS(pkt->data_len + sizeof(struct udp_hdr_t));
            pkt->transport.size = sizeof(struct udp_hdr_t);
            buf += sizeof(struct udp_hdr_t);
            buf -= sizeof(struct udp_hdr_t);

            // copy data
            if (len < pkt->data_len) {
                kprint(KPRN_WARN, "netstack: udp: not enough space in frame to send packet");
                errno = ENOBUFS;
                return -1;
            }
            memcpy(buf, pkt->data, pkt->data_len);

            // checksum
            if (pkt->nic->flags & NIC_TX_UDP_CS) {
                pkt->nic_flags |= PKT_FLAG_UDP_CS;
            } else {
                kprint(KPRN_ERR, "netstack: udp: TODO: Implement checksum calculation");
                return -1;
            }
        } return 0;

        // for unknown type assume raw
        default: {
            if (len < pkt->data_len) {
                errno = ENOBUFS;
                return -1;
            }
            memcpy(buf, pkt->data, pkt->data_len);
        } return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Network layer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int process_network(struct packet_t* pkt, char* buf, size_t len) {
    switch (pkt->datalink.type) {
        // handle ipv4
        case ETHER_IPV4: {
            if (len < sizeof(struct ipv4_hdr_t)) {
                kprint(KPRN_WARN, "netstack: ipv4: not enough space in frame to send packet");
                errno = ENOBUFS;
                return -1;
            }

            // construct the header
            struct ipv4_hdr_t* hdr = (struct ipv4_hdr_t *) buf;
            hdr->ver = 4;
            hdr->head_len = sizeof(struct ipv4_hdr_t) / 4;
            hdr->fragment = 0;
            hdr->protocol = pkt->network.type;
            hdr->id = 0;
            hdr->tos = 0;
            hdr->ttl = 64;
            hdr->src = pkt->network.src;
            hdr->dst = pkt->network.dst;

            pkt->network.size = sizeof(struct ipv4_hdr_t);
            buf += sizeof(struct ipv4_hdr_t);
            len -= sizeof(struct ipv4_hdr_t);

            // process transport
            int err = process_transport(pkt, buf, len);
            if (err < 0) {
                return err;
            }

            // update the total size in the header
            hdr->total_len = HTONS(pkt->data_len + pkt->transport.size + sizeof(struct ipv4_hdr_t));

            // checksum
            if (pkt->nic->flags & NIC_TX_IP_CS) {
                pkt->nic_flags |= PKT_FLAG_IP_CS;
            } else {
                kprint(KPRN_ERR, "netstack: network: TODO: Implement checksum calculation");
                return -1;
            }
        } return 0;

        // for unknown type assume raw
        default: {
            if (len < pkt->data_len) {
                kprint(KPRN_WARN, "netstack: network: not enough space in frame to send packet");
                errno = ENOBUFS;
                return -1;
            }
            memcpy(buf, pkt->data, pkt->data_len);
        } return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Datalink layer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int process_datalink(struct packet_t* pkt, char* buf, size_t len) {
    if (len < sizeof(struct ether_hdr)) {
        kprint(KPRN_WARN, "netstack: datalink: not enough space in frame to send packet");
        errno = ENOBUFS;
        return -1;
    }

    struct ether_hdr* hdr = (struct ether_hdr *) buf;
    hdr->src = pkt->datalink.src;
    hdr->dst = pkt->datalink.dst;
    hdr->type = pkt->datalink.type;
    pkt->datalink.size = sizeof(struct ether_hdr);

    buf += sizeof(struct ether_hdr);
    len -= sizeof(struct ether_hdr);

    return process_network(pkt, buf, len);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// High level send-frame
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: handle local-host mappings
//       maybe do that by just passing the packet_t to the nic
//       and it will decide what to do wiGth it next?
int netstack_send_frame(struct packet_t* pkt) {
    // allocate the buffers
    void* packet = kalloc(1536u);

    // format it
    int err = process_datalink(pkt, packet, 1536u);
    if (err < 0) {
        return err;
    }

    // send it
    size_t total_size = pkt->network.size + pkt->datalink.size + pkt->transport.size + pkt->data_len;
    return pkt->nic->calls.send_packet(pkt->nic->internal_fd, packet, total_size, pkt->nic_flags);
}
