
#include <lib/klib.h>
#include <net/proto/ether.h>
#include <lib/cmem.h>
#include <lib/errno.h>
#include "net.h"
#include "netstack.h"

static int process_network(struct packet_t* pkt, char* buf, size_t len) {
    switch (pkt->datalink.type) {
        // TODO: IPv4

        // by default just copy the data and call it a day
        default:
            if(len < pkt->data_len) {
                errno = ENOBUFS;
                return -1;
            }
            memcpy(buf, pkt->data, pkt->data_len);
            return 0;
    }
}

static int process_datalink(struct packet_t* pkt, char* buf, size_t len) {
    if (len < sizeof(struct ether_hdr)) {
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

int netstack_send_frame(struct packet_t* pkt) {
    // allocate the buffers
    void* packet = kalloc(1536u);

    // format it
    int err = process_datalink(pkt, packet, 1536u);
    if (err < 0) {
        return err;
    }

    // send it
    // we assume fcs is added automatically
    size_t total_size = pkt->data_len + pkt->network.size + pkt->datalink.size + pkt->transport.size;
    return pkt->nic->calls.send_packet(pkt->nic->internal_fd, packet, total_size);
}
