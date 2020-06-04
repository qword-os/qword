#ifndef __TCP_H__
#define __TCP_H__

/* define each flag simply as 1 << N where N is the offset of the bit
 * from the bitfield detailed in tcp_hdr_t.
 * if the Nth bit in the `flags` variable is set,
 * that flag is on */
#define TCP_CWR (1 << 0)
#define TCP_ECE (1 << 1)
#define TCP_URG (1 << 2)
#define TCP_ACK (1 << 3)
#define TCP_PSH (1 << 4)
#define TCP_RST (1 << 5)
#define TCP_SYN (1 << 6)
#define TCP_FIN (1 << 7)

/* while this is called a header here,
 * it more properly corresponds to the
 * tcp segment: header + data */
struct tcp_hdr_t {
    uint16_t source;
    uint16_t dest;
    uint32_t seq_num;
    uint32_t ack_seq_num;
    /* 6 bits reserved */
    /* DOFF: data offset */
    /* FIN: sender data stream finished */
    /* SYN: sync sequence numbers */
    /* RST: reset connection */
    /* PSH: push function */
    /* ACK: ack_seq_num is significant */
    /* URG: urg_ptr significant */
    uint16_t res1:4, doff:4, fin:1, syn:1,
             rst:1, psh:1, ack:1, urg:1,
             ece:1, cwr:1;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
    char data[];
};

/* construct a tcp header with specified parameters */
/* TODO: socket stuff */
void tcp_new(int fd, struct packet_t *packet, int flags, const void *buf, size_t len);

#endif
