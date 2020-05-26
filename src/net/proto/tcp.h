#ifndef __TCP_H__
#define __TCP_H__

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
};

#endif
