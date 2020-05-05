#ifndef XHCI_H
#define XHCI_H

#include <lib/lock.h>
#include <usb/usb.h>

#define BIT(x) (1 << (x))

#define XHCI_CLASS    0x0C
#define XHCI_SUBCLASS 0x03
#define PROG_IF       0x30

#define XHCI_CONFIG_MAX_SLOT 44
#define XHCI_IRS_MAX         1024

#define TRB_MAX_BUFF_SHIFT    16
#define TRB_MAX_BUFF_SIZE     (1 << TRB_MAX_BUFF_SHIFT)
#define TRB_CHAIN             (1 << 4)
#define TRB_IOC               (1 << 5)
#define TRB_INTR_TARGET_SHIFT (22)
#define TRB_INTR_TARGET_MASK  (0x3ff)
#define TRB_LEN_MASK          (0x1ffff)

#define TRB_NORMAL           1
#define TRB_SETUP_STAGE      2
#define TRB_DATA_STAGE       3
#define TRB_STATUS_STAGE     4
#define TRB_ISOCH            5
#define TRB_LINK             6
#define TRB_EVENT_DATA       7
#define TRB_NOOP             8
#define TRB_ENABLE_SLOT      9
#define TRB_DISABLE_SLOT     10
#define TRB_ADDRESS_DEV      11
#define TRB_CONFIG_EP        12
#define TRB_EVAL_CNTX        13
#define TRB_TRANSFER_EVENT   32
#define TRB_CMD_COMPLETION   33
#define TRB_PORT_STATUS      34
#define XHCI_CTX_TYPE_DEVICE 0x1
#define XHCI_CTX_TYPE_INPUT  0x2
#define TRB_SLOT_ID(x)       (((x) & (0xFF << 24)) >> 24)
#define TRB_CMD_SLOT_ID(x)   ((x & 0xFF) << 24)
#define TRB_TYPE(x)          (((x) & (0x3F << 10)) >> 10)
#define TRB_CMD_TYPE(x)      ((x & 0x3F) << 10)
#define TRB_STATUS(x)        (((x) & (0xFF << 24)) >> 24)
#define TRB_ADDR_LOW(x)      ((uint32_t)((uint64_t)(x)))
#define TRB_ADDR_HIGH(x)     ((uint32_t)((uint64_t)(x) >> 32))
#define TRB_TRT(x)           (((x)&0x3) << 16)
#define TRB_DIR_IN           BIT(16)
#define TRB_IDT              BIT(6)
#define MAX_PACKET_SIZE(x)   (((x)&0xFFFF) << 16)
#define MAX_BURST(x)         (((x)&0xFF) << 8)
#define EP_TYPE(x)           (((x)&0x07) << 3)
#define EP_ISOC_OUT          1
#define EP_BULK_OUT          2
#define EP_INT_OUT           3
#define EP_CTRL              4
#define EP_ISOC_IN           5
#define EP_BULK_IN           6
#define EP_INT_IN            7
#define RESET_CHANGE_BITS                                                  \
    (1 << 9) | (1 << 21) | (1 << 20) | (1 << 19) | (1 << 18) | (1 << 17) | \
        (1 << 22) | (1 << 23)

#define TRB_STATUS(x)  (((x) & (0xFF << 24)) >> 24)
#define TRB_SLOT_ID(x) (((x) & (0xFF << 24)) >> 24)

#define LAST_CONTEXT(x)  (x << 27)
#define ROOT_HUB_PORT(x) (((x)&0xff) << 16)

struct usb_hc_t *usb_init_xhci(void);

struct xhci_cap_regs {
    uint8_t caplength;
    uint8_t rsvd;
    uint16_t version;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} __attribute__((packed));

struct xhci_port_regs {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
    uint32_t reserved;
} __attribute__((packed, aligned(4)));

struct xhci_op_regs {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t page_size;
    uint8_t rsvd1[0x14 - 0x0C];
    uint32_t dnctrl;
    uint64_t crcr;
    uint8_t rsvd2[0x30 - 0x20];
    uint64_t dcbaap;
    uint32_t config;
    uint8_t reserved2[964]; /* 3C - 3FF */
    volatile struct xhci_port_regs prs[256];
} __attribute__((packed, aligned(8)));

struct xhci_int_regs {
    uint32_t iman;
    uint32_t imod;
    uint32_t erstsz;
    uint32_t reserved;
    uint64_t erstba;
    uint64_t erdp;
} __attribute__((packed, aligned(8)));

struct xhci_run_regs {
    uint32_t mfindex; /* microframe index */
    uint8_t reserved[28];
    volatile struct xhci_int_regs irs[XHCI_IRS_MAX];
} __attribute__((packed, aligned(8)));

struct xhci_db_regs {
    uint32_t db[256];
} __attribute__((packed, aligned(4)));

struct xhci_transfer_trb {
    uint64_t addr;
    uint32_t len;
    uint32_t flags;
} __attribute__((packed));

struct xhci_link_trb {
    uint64_t addr;
    uint32_t field2;
    uint32_t field3;
} __attribute__((packed));

/* Event TRB */
struct xhci_event_trb {
    uint64_t addr;
    uint32_t status;
    uint32_t flags;
} __attribute__((packed));

struct xhci_event {
    struct xhci_event_trb trb;
    int event;
};

struct xhci_command_trb {
    uint32_t field[4];
} __attribute__((packed));

typedef struct XHCITRB {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
    uint64_t addr;
    int ccs;
} XHCITRB;

union xhci_trb {
    struct xhci_event_trb event;
    struct xhci_transfer_trb xfer;
    struct xhci_command_trb cmd;
    struct xhci_link_trb link;
};

enum xhci_seg_type {
    TYPE_CTRL = 0,
    TYPE_BULK,
    TYPE_COMMAND,
    TYPE_EVENT,
};

#define TRB_NORMAL         1
#define TRB_SETUP_STAGE    2
#define TRB_DATA_STAGE     3
#define TRB_STATUS_STAGE   4
#define TRB_ISOCH          5
#define TRB_LINK           6
#define TRB_EVENT_DATA     7
#define TRB_NOOP           8
#define TRB_ENABLE_SLOT    9
#define TRB_DISABLE_SLOT   10
#define TRB_ADDRESS_DEV    11
#define TRB_CONFIG_EP      12
#define TRB_EVAL_CNTX      13
#define TRB_TRANSFER_EVENT 32
#define TRB_CMD_COMPLETION 33
#define TRB_PORT_STATUS    34
#define TRB_CMD_TYPE(x)    ((x & 0x3F) << 10)
#define EP_TYPE(x)         (((x)&0x07) << 3)
#define EP_ISOC_OUT        1
#define EP_BULK_OUT        2
#define EP_INT_OUT         3
#define EP_CTRL            4
#define EP_ISOC_IN         5
#define EP_BULK_IN         6
#define EP_INT_IN          7
#define MAX_PACKET_SIZE(x) (((x)&0xFFFF) << 16)
#define MAX_BURST(x)       (((x)&0xFF) << 8)
#define ERROR_COUNT(x)     (((x)&0x03) << 1)
#define TRB_TYPE(x)        (((x) & (0x3F << 10)) >> 10)
#define TRB_CMD_TYPE(x)    ((x & 0x3F) << 10)

struct xhci_seg {
    union xhci_trb *trbs;
    size_t trbs_dma;
    struct xhci_seg *next;
    uint64_t enq;
    uint64_t deq;
    uint32_t size;
    uint32_t cycle_state;
    enum xhci_seg_type type;
    struct xhci_event **seg_events;
    lock_t lock;
    // TODO reduce number
};

struct xhci_erst_entry {
    uint64_t addr;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed, aligned(8)));

struct xhci_erst {
    struct xhci_erst_entry *entries;
    uint64_t dma;
    uint32_t num_segs; /* number of segments */
};

struct xhci_control_ctx {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
} __attribute__((packed));

struct xhci_slot_ctx {
    uint32_t field1;
    uint32_t field2;
    uint32_t field3;
    uint32_t field4;
    uint32_t reserved[4];
} __attribute__((packed));

struct xhci_ep_ctx {
    uint32_t field1;
    uint32_t field2;
    uint64_t deq_addr;
    uint32_t field4;
    uint32_t reserved[3];
} __attribute__((packed));

struct xhci_ctx {
    uint8_t type;
    uint32_t size;
    uint8_t *addr;
    uint64_t dma_addr;
} __attribute__((packed, aligned(64)));

struct xhci_dev {
    void *xhci_controller;
    uint32_t slot_id;
    struct xhci_ctx in_ctx;
    struct xhci_ctx out_ctx;
    struct xhci_seg control;
    uint32_t ctx_size;
    struct xhci_seg *ep_segs[16];
};

struct xhci_endpoint {
    struct xhci_seg seg;
    int x_epno;
    lock_t lock;
};

struct xhci_port_speed_id {
    uint8_t value;
    uint8_t exponent;
    uint8_t type;

    int full_duplex;

    uint8_t link_protocol;

    uint16_t mantissa;
};

struct xhci_port_protocol {
    int minor;
    int major;

    char name[5];

    uint8_t compatible_port_off;
    uint8_t compatible_port_count;

    uint8_t protocol_specific;

    uint8_t protocol_slot_type;

    struct xhci_port_speed_id speeds[16];
};

struct xhci_hcd {
    volatile struct xhci_cap_regs *cap_regs;
    volatile struct xhci_op_regs *op_regs;
    volatile struct xhci_run_regs *run_regs;
    volatile struct xhci_db_regs *db_regs;
    struct xhci_dev **xdevs;
    volatile uint64_t *dcbaap;
    size_t dcbaap_dma;
    struct xhci_seg ering;
    struct xhci_seg crseg;
    struct xhci_erst erst;
    uint32_t erds_size;
    uint32_t slot_id;
    uint32_t context_size;
    uint64_t *scratchpad_buffer_array;

    int num_protcols;
    struct xhci_port_protocol protocols[255];

    int irq_line;
    int port_events[XHCI_CONFIG_MAX_SLOT + 1];
};

struct usb_hc_t *usb_init_xhci(void);

#endif