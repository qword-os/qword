#include <stdint.h>
#include <stddef.h>
#include <net/proto/tcp.h>
#include <net/socket.h>
#include <net/net.h>
#include <net/proto/ether.h>
#include <net/proto/ipv4.h>

/* TODO: the packet stuff is pretty superfluous now, slim down struct packet_t ? */
/* constructing a tcp segment: ethernet header -> ipv4 header -> tcp header */
void tcp_new(int fd, struct packet_t *pkt, int flags, const void *tcp_data, size_t data_len) {
    /* could abstract away socket resolving */
    struct socket_descriptor_t *sock = socket_from_fd(fd);

    struct ether_hdr *ether = (struct ether_hdr *)pkt->buf;
    ether->type = ETHER_IPV4;

    struct ipv4_hdr_hdr_t *ipv4 = (struct ipv4_hdr_t *)(pkt->buf + sizeof(struct ether_hdr));

    /* ignore checksum and total length till i understand more what to do with them */
    ipv4_hdr->ver = 4;
    ipv4_hdr->head_len = sizeof(struct ipv4_hdr_t) / 4;
    ipv4_hdr->tos = 0; /* TOS/DSCP: we don't need this */
    /* total size of ip datagram: header + tcp header + tcp data */
    ipv4_hdr->total_len = sizeof(ipv4_hdr_t) + sizeof(tcp_hdr_t) + data_len;
    ipv4_hdr->id = NTOHS(sock->ipid); /* TODO these capitals look like wank */
    ipv4_hdr->protocol = PROTO_TCP;
    ipv4_hdr->frag_flag = HTONS(IPV4_HEAD_DF_MASK);
    ipv4_hdr->ttl = 64;
    ipv4_hdr->src = sock->source_ip;
    ipv4_hdr->dest = sock->dest_ip;

    /* tcp header 20 bytes after start of ipv4_hdr header */
    struct tcp_hdr_t *tcp_hdr = (struct tcp_hdr_t *)((void *)ipv4_hdr  + (ipv4->head_len * 4));

    tcp->source = sock->source_port;
    tcp->dest = sock->dest_port;

    /* TODO: sequence numbers and other crap */
    /* tcp header is 20 bytes wide */
    tcp->doff = 5;
    tcp->res1 = 0;

    tcp->fin = (flags & TCP_FIN);
    tcp->syn = (flags & TCP_SYN);
    tcp->rst = (flags & TCP_RST);
    tcp->psh = (flags & TCP_PSH);
    tcp->ack = (flags & TCP_ACK);
    tcp->urg = (flags & TCP_URG);
    tcp->ece = (flags & TCP_ECE);
    tcp->cwr = (flags & TCP_CWR);

    tcp->window = HTONS(0x1000);
    tcp->checksum = 0; /* TODO how does one do the tcp checksum */
    tcp->urg_ptr = 0;

    /* copy data */
    memcpy(tcp->data, data, data_len);
    /* tcpchecksum(pkt) */
    pkt->pkt_len = ntohs(ipv4_hdr->total_length);
}
