#ifndef __RTL81X9_PRIV_H__
#define __RTL81X9_PRIV_H__

#include <stdint.h>

#define RTL_VENDOR_ID 0x10ec

/* Mac Address */
#define IDR0            (0x0000)
#define IDR1            (0x0001)
#define IDR2            (0x0002)
#define IDR3            (0x0003)
#define IDR4            (0x0004)
#define IDR5            (0x0005)

/* Multicase Address */
#define MAR0            (0x0008)
#define MAR1            (0x0007)
#define MAR2            (0x000a)
#define MAR3            (0x000b)
#define MAR4            (0x000c)
#define MAR5            (0x000d)
#define MAR6            (0x000e)
#define MAR7            (0x000f)

/* TX ring */
#define TNPDS_LOW       (0x0020)
#define TNPDS_HIGH      (0x0024)

/* TX high priority ring */
#define THPDS_LOW       (0x0028)
#define HNPDS_HIGH      (0x003b)

/* Command Registe */
#define CR              (0x0037)
#define     RST             (1u << 4u) /* Reset */
#define     RE              (1u << 3u) /* Receiver enable */
#define     TE              (1u << 2u) /* Transmit enable */

/* Transmit polling */
#define TPPoll          (0x0038)
#define TPPoll_8139     (0x00d9) /* The rtl8139 has a different register */
#define     HQP             (1u << 7u) /* Trigger high priority queue */
#define     NPQ             (1u << 6u) /* Trigger normal priority queue */

/* Interrupt Mask and Status registers */
#define IMR             (0x003C)
#define ISR             (0x003E)
#define     ROK             (1u << 0u) /* Rx Ok Interrupt */
#define     RER             (1u << 1u) /* Rx Error Interrupt */
#define     TOK             (1u << 2u) /* Tx Ok Interrupt */
#define     TER             (1u << 3u) /* Tx Error Interrupt */
#define     RDU             (1u << 4u) /* Rx Buffer Overflow */
#define     PUN             (1u << 5u) /* Packet Underrun */
#define     FOVW            (1u << 6u) /* Rx Fifo Overflow */
#define     TDU             (1u << 7u) /* Tx Descriptor Unavailable */
#define     SWInt           (1u << 8u) /* Software Interrupt */
#define     LenChg_8139     (1u << 13u) /* Cable Length Change */
#define     TimeOut         (1u << 14u) /* Time out */
#define     SERR            (1u << 15u) /* System Error */

/* TX and RX configuration registers */
#define TCR             (0x0040)
#define     IFG_NORMAL      (0b11u << 24u) /* InterFrameGap Time */
#define     CRC             (1u << 16u) /* Append CRC */

#define RCR             (0x0044)
#define     RXFTH_NONE      (0b111u << 13u) /* no rx threshold */
#define     MXDMA_UNLIMITED (0b111u << 8u) /* no mac size of dma data burst */
#define     AB              (1u << 3u) /* Accept Broadcast Packets */
#define     APM             (1u << 1u) /* Accept Physical Match Packets */

/* Configuration registers */
#define CR9346          (0x0050)
#define     CR9346_Unlock   (0b11u << 6u) /* Unlock config registers */
#define     CR9346_Lock     (0u) /* Lock config registers */
#define CONFIG0         (0x0051)
#define CONFIG1         (0x0052)
#define CONFIG2         (0x0053)
#define CONFIG3         (0x0054)
#define CONFIG4         (0x0055)
#define CONFIG5         (0x0056)

/* RX packet max size */
#define RMS             (0x00DA)
#define RMS_MASK        (0xe)

#define CPCR            (0x00E0)
/* rtl81x9 */
#define     RxChkSum        (1u << 5u) /*  */
/* rtl8139 */
#define     CPRx            (1u << 1u) /* Receive enable */
#define     CPTx            (1u << 0u) /* Transmit enable */

/* RX ring */
#define RDSAR_LOW       (0x00E4)
#define RDSAR_HIGH      (0x00E8)

#define MTPS            (0x00EC)

struct __attribute((packed)) pkt_desc_t {
    uint32_t opts1;
#define FRAME_LENGTH_MASK 0xfff
#define OWN     (1u << 31u)
#define EOR     (1u << 30u)
#define FS      (1u << 29u)
#define LS      (1u << 28u)
    // RX ONLY
    // for rtl8139
    #define IPF_RTL8139     (1u << 15u)
    #define UDPF_RTL8139    (1u << 14u)
    #define TCPF_RTL8139    (1u << 13u)
    #define IPF             (1u << 16u)
    #define UDPF            (1u << 15u)
    #define TCPF            (1u << 14u)

    // TX ONLY
    #define IPCS    (1u << 18u)
    #define UDPCS   (1u << 17u)
    #define TCPCS   (1u << 16u)

    uint32_t opts2;
    uint64_t buff;
};

// as defined by the spec
#define RX_MAX_ENTRIES 1024
#define TX_MAX_ENTRIES 1024

#endif //__RTL81X9_PRIV_H__
