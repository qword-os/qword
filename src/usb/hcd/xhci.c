#include <lib/alloc.h>
#include <lib/cio.h>
#include <lib/cmem.h>
#include <lib/dynarray.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <mm/mm.h>
#include <sys/apic.h>
#include <sys/idt.h>
#include <sys/panic.h>
#include <sys/pci.h>
#include <usb/hcd/xhci.h>

#define DEBUG_EVENTS 0

struct usb_hc_t *xhci_controller;

static uint32_t xhci_get_epno(int epno, int dir) {
    uint32_t x_epno;
    x_epno = (dir ? 1 : 0) + 2 * epno;
    return x_epno;
}

static int xhci_setup_seg(struct xhci_seg *seg, uint32_t size, uint32_t type) {
    void *addr = kalloc(size);
    seg->trbs = addr;
    seg->trbs_dma = (size_t)seg->trbs - MEM_PHYS_OFFSET;
    struct xhci_link_trb *link;

    seg->size = size / 16;
    seg->next = NULL;
    seg->type = type;
    seg->cycle_state = 1;
    seg->enq = (uint64_t)seg->trbs;
    seg->deq = (uint64_t)seg->trbs;
    memset((void *)seg->trbs, 0, size);

    if (type != TYPE_EVENT) {
        link = (struct xhci_link_trb *)(seg->trbs + seg->size - 1);
        link->addr = seg->trbs_dma;
        link->field2 = 0;
        link->field3 = (0x1 | TRB_CMD_TYPE(TRB_LINK));
        seg->seg_events = kalloc(sizeof(struct xhci_event) * 4096);
    }
    seg->lock = new_lock;
    return 1;
}

static void *xhci_get_trb(volatile struct xhci_seg *seg) {
    uint64_t enq;
    uint32_t index;
    struct xhci_link_trb *link;

    enq = seg->enq;
    index = (enq - (uint64_t)seg->trbs) / 16 + 1;
    // Check if we should cycle back to the beginning
    if (index == (seg->size - 1)) {
        seg->enq = (uint64_t)seg->trbs;
        seg->cycle_state ^= seg->cycle_state;
        link = (struct xhci_link_trb *)(seg->trbs + seg->size - 1);
        link->addr = (seg->trbs_dma);
        link->field2 = 0;
        link->field3 = (0x1 | TRB_CMD_TYPE(TRB_LINK));
    } else {
        seg->enq = seg->enq + 16;
    }
    return (void *)enq;
}

static void fill_trb_buf(volatile struct xhci_command_trb *cmd, uint32_t field1,
                         uint32_t field2, uint32_t field3, uint32_t field4,
                         int first) {
    uint32_t val, cycle_state;

    cmd->field[0] = (field1);
    cmd->field[1] = (field2);
    cmd->field[2] = (field3);

    if (!first) {
        val = (cmd->field[3]);
        cycle_state = (val & 0x1) ? 0 : 1;
        val = cycle_state | (field4 & ~0x1);
        cmd->field[3] = (val);
    } else {
        cmd->field[3] = field4;
    }

    return;
}

static int xhci_send_command(struct xhci_hcd *controller, uint32_t field1,
                             uint32_t field2, uint32_t field3, uint32_t field4,
                             struct xhci_event *ev) {

    volatile struct xhci_db_regs *dbr;

    dbr = controller->db_regs;

    uint32_t index =
        (controller->crseg.enq - (uint64_t)controller->crseg.trbs) / 0x10;
    struct xhci_command_trb *command = xhci_get_trb(&controller->crseg);
    fill_trb_buf(command, field1, field2, field3, field4, 0);
    controller->crseg.seg_events[index] = ev;

    dbr->db[0] = 0;

    return 0;
}

int xhci_enable_slot(struct xhci_hcd *controller) {
    uint32_t field1 = 0, field2 = 0, field3 = 0,
             field4 = TRB_CMD_TYPE(TRB_ENABLE_SLOT);
    struct xhci_event ev = {0};
    xhci_send_command(controller, field1, field2, field3, field4, &ev);
    event_await(&ev.event);
    if (TRB_STATUS(ev.trb.status)) {
        int slot = TRB_SLOT_ID(ev.trb.flags);
        controller->slot_id = slot;
        return 0;
    } else {
        kprint(KPRN_INFO, "usb/xhci: failed to enable slot");
        controller->slot_id = 0;
        return 1;
    }
}

static int xhci_send_addr_device(struct xhci_hcd *xhcd, uint32_t slot_id,
                                 uint64_t dma_in_ctx, int bsr) {
    uint32_t field1, field2, field3, field4;

    field1 = TRB_ADDR_LOW(dma_in_ctx) & ~0xF;
    field2 = TRB_ADDR_HIGH(dma_in_ctx);
    field3 = 0;
    field4 =
        TRB_CMD_TYPE(TRB_ADDRESS_DEV) | TRB_CMD_SLOT_ID(slot_id) | (bsr << 9);
    struct xhci_event ev = {0};
    xhci_send_command(xhcd, field1, field2, field3, field4, &ev);
    event_await(&ev.event);
    if ((ev.trb.status >> 24) != 1) {
        kprint(KPRN_INFO,
               "usb/xhci: Error while attempting to address device: %X",
               ev.trb.status);
        return 1;
    } else {
        return 0;
    }
}

static int xhci_configure_ep(struct xhci_hcd *xhcd, uint32_t slot_id,
                             uint64_t dma_in_ctx) {
    uint32_t field1, field2, field3, field4;

    field1 = TRB_ADDR_LOW(dma_in_ctx) & ~0xF;
    field2 = TRB_ADDR_HIGH(dma_in_ctx);
    field3 = 0;
    field4 = TRB_CMD_TYPE(TRB_CONFIG_EP) | TRB_CMD_SLOT_ID(slot_id);
    struct xhci_event ev = {0};
    xhci_send_command(xhcd, field1, field2, field3, field4, &ev);
    event_await(&ev.event);
    if ((ev.trb.status >> 24) != 1) {
        kprint(KPRN_INFO,
               "usb/xhci: Error while attempting to configure endpoint");
        return 1;
    } else {
        return 0;
    }
}

/*
 * Context management
 */

void xhci_setup_context(struct xhci_ctx *ctx, uint32_t size, uint32_t type) {
    void *addr = kalloc(size);
    ctx->addr = (uint8_t *)addr;
    ctx->dma_addr = (size_t)ctx->addr - MEM_PHYS_OFFSET;
    ctx->type = type;
    ctx->size = size;
}

static struct xhci_slot_ctx *xhci_get_slot_ctx(struct xhci_ctx *ctx,
                                               uint32_t ctx_size) {
    uint32_t offset = 0;

    if (ctx->type == XHCI_CTX_TYPE_INPUT)
        offset += ctx_size;
    return (struct xhci_slot_ctx *)((size_t)ctx->addr + offset);
}

static struct xhci_control_ctx *xhci_get_control_ctx(struct xhci_ctx *ctx) {
    if (ctx->type == XHCI_CTX_TYPE_INPUT)
        return (struct xhci_control_ctx *)ctx->addr;
    return NULL;
}

static struct xhci_ep_ctx *xhci_get_ep0_ctx(struct xhci_ctx *ctx,
                                            uint32_t ctx_size) {
    uint32_t offset = ctx_size;
    int mul = 1;

    if (ctx->type == XHCI_CTX_TYPE_INPUT) {
        mul++;
    }
    return (struct xhci_ep_ctx *)(ctx->addr + offset * mul);
}

static int xhci_send_control(struct usb_dev_t *device, void *data,
                             struct usb_request_t request, int toDevice) {
    struct xhci_command_trb *command;

    struct xhci_hcd *controller = device->internal_controller;
    struct xhci_dev *dev = controller->xdevs[device->hcd_devno];
    struct xhci_seg *ctrl = &dev->control;
    volatile struct xhci_db_regs *dbr = controller->db_regs;
    uint32_t slot_id = dev->slot_id;

    command = xhci_get_trb(ctrl);
    struct xhci_command_trb *first = command;

    // We want to keep this value for later
    uint32_t val, cycle_state;
    val = (command->field[3]);
    cycle_state = (val & 0x1) ? 0 : 1;

    fill_trb_buf(command,
                 request.request_type | (request.request << 8) |
                     (request.value << 16),
                 request.length << 16 | request.index, 8,
                 ((toDevice ? 2 : 3) << 16) | (1 << 6) |
                     (TRB_SETUP_STAGE << 10) | (1 << 5),
                 1);

    if (request.length) {
        command = xhci_get_trb(ctrl);
        size_t addr = ((size_t)data) - MEM_PHYS_OFFSET;

        fill_trb_buf(command, addr & 0xFFFFFFFF, addr >> 32, request.length,
                     (1 << 2) | (!toDevice << 16) | (TRB_DATA_STAGE << 10) |
                         (1 << 5),
                     0);
    }

    command = xhci_get_trb(ctrl);

    fill_trb_buf(command, 0, 0, 0, (1 << 5) | (TRB_STATUS_STAGE << 10), 0);

    uint32_t index = (ctrl->enq - (uint64_t)ctrl->trbs) / 0x10;
    struct xhci_event ev = {0};

    ctrl->seg_events[index] = &ev;
    first->field[3] |= cycle_state;

    dbr->db[slot_id] = 1;
    event_await(&ev.event);
    return (ev.trb.status >> 24) != 1;
}

static int xhci_get_descriptor(struct usb_dev_t *device, uint8_t desc_type,
                               uint8_t desc_index, void *dest, uint16_t size) {
    struct usb_request_t req = {0};
    req.length = size;
    req.request_type = 0b10000000;
    req.request = 6;
    req.value = desc_type << 8 | desc_index;
    return xhci_send_control(device, dest, req, 0);
}

int xhci_init_device(struct xhci_hcd *controller, uint32_t port,
                     struct xhci_port_protocol proto) {
    xhci_enable_slot(controller);
    if (!controller->slot_id) {
        kprint(KPRN_INFO, "usb/xhci: failed to get slot id.");
    }
    kprint(KPRN_INFO, "usb/xhci: Initializing device, slot id: id: %x port: %x",
           controller->slot_id, port);
    uint32_t slot_id = controller->slot_id;

    struct xhci_dev *xhci_dev = kalloc(sizeof(xhci_dev));
    struct xhci_slot_ctx *slot;
    struct xhci_control_ctx *ctrl;
    struct xhci_ep_ctx *ep0;
    uint16_t max_packet = 0;

    xhci_setup_context(&xhci_dev->in_ctx, 4096, XHCI_CTX_TYPE_INPUT);
    ctrl = xhci_get_control_ctx(&xhci_dev->in_ctx);
    ctrl->add_flags = (0x3);
    ctrl->drop_flags = 0;

    slot = xhci_get_slot_ctx(&xhci_dev->in_ctx, controller->context_size);

    // TODO hub support

    slot->field1 = (1 << 27) | ((0x4 << 10) << 10);
    slot->field2 = ROOT_HUB_PORT(port + 1);

    ep0 = xhci_get_ep0_ctx(&xhci_dev->in_ctx, controller->context_size);

    volatile struct xhci_port_regs *port_regs = &controller->op_regs->prs[port];
    uint8_t port_speed = (port_regs->portsc >> 10) & 0b1111;
    switch (port_speed) {
        case 1:
            // TODO handle full speed
            kprint(KPRN_ERR, "usb/xhci: full speed devices are not handled");
            // TODO set a temporary max transfer size and read the first 8 bytes
            // of the device descriptor
            return -1;
        case 2:
            max_packet = 8;
            break;
        case 3:
            max_packet = 64;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            max_packet = 512;
            break;
    }

    uint32_t val = EP_TYPE(EP_CTRL) | MAX_BURST(0) | ERROR_COUNT(3) |
                   MAX_PACKET_SIZE(max_packet);
    ep0->field2 = val;
    // Average trb length is always 8.
    ep0->field4 = 8;
    xhci_setup_seg(&xhci_dev->control, 4096, TYPE_CTRL);
    ep0->deq_addr =
        (xhci_dev->control.trbs_dma | xhci_dev->control.cycle_state);

    xhci_setup_context(&xhci_dev->out_ctx, 4096, XHCI_CTX_TYPE_DEVICE);
    controller->dcbaap[slot_id] = xhci_dev->out_ctx.dma_addr;
    slot = xhci_get_slot_ctx(&xhci_dev->out_ctx, controller->context_size);
    xhci_send_addr_device(controller, slot_id, xhci_dev->in_ctx.dma_addr, 0);
    ctrl->add_flags = (0x1);
    xhci_configure_ep(controller, slot_id, xhci_dev->in_ctx.dma_addr);

    struct usb_dev_t udevice = {0};
    udevice.max_packet_size = 512;
    udevice.controller = xhci_controller;
    xhci_dev->xhci_controller = controller;
    udevice.internal_controller = controller;
    controller->xdevs[slot_id] = xhci_dev;
    xhci_dev->slot_id = slot_id;

    if (usb_add_device(udevice, slot_id))
        return -1;
    return 0;
}

static struct xhci_ep_ctx *xhci_get_ep_ctx(volatile struct xhci_ctx *ctx,
                                           uint32_t ctx_size, uint32_t epno) {

    if (ctx->type == XHCI_CTX_TYPE_INPUT)
        epno++;
    return (struct xhci_ep_ctx *)(ctx->addr + epno * ctx_size);
}

int xhci_setup_endpoint(struct usb_dev_t *dev, int address, int max_packet) {
    struct xhci_endpoint *ep = kalloc(sizeof(struct xhci_endpoint));
    ep->lock = new_lock;
    struct xhci_hcd *controller = dev->internal_controller;
    struct xhci_dev *xdev = controller->xdevs[dev->hcd_devno];
    struct xhci_control_ctx *ctrl;
    struct xhci_ep_ctx *ep_ctx;

    // Copy context info from output to input.
    volatile struct xhci_slot_ctx *out_slot =
        xhci_get_slot_ctx(&xdev->out_ctx, controller->context_size);
    volatile struct xhci_slot_ctx *in_slot =
        xhci_get_slot_ctx(&xdev->in_ctx, controller->context_size);
    in_slot->field1 = out_slot->field1;
    in_slot->field2 = out_slot->field2;
    in_slot->field3 = out_slot->field3;
    in_slot->field4 = out_slot->field4;

    struct xhci_ep_ctx *ep0_ctx_out =
        xhci_get_ep0_ctx(&xdev->out_ctx, controller->context_size);
    struct xhci_ep_ctx *ep0_ctx_in =
        xhci_get_ep0_ctx(&xdev->out_ctx, controller->context_size);
    ep0_ctx_in->field1 = ep0_ctx_out->field1;
    ep0_ctx_in->field2 = ep0_ctx_out->field2;
    ep0_ctx_in->deq_addr = ep0_ctx_out->deq_addr;
    ep0_ctx_in->field4 = ep0_ctx_out->field4;

    int out = 1;
    int endpoint_no = address & 0x0F;
    if (address & 0x80) {
        out = 0;
    }

    struct xhci_seg new_seg;
    xhci_setup_seg(&new_seg, 4096, TYPE_BULK);

    int x_epno = xhci_get_epno(endpoint_no, !out);

    in_slot->field1 &= ~((0x1f << 27));
    in_slot->field1 |= ((x_epno + 1) << 27);

    // TODO add support for other kinds of endpoints
    if (out) {
        ctrl = xhci_get_control_ctx(&xdev->in_ctx);
        ep_ctx =
            xhci_get_ep_ctx(&xdev->in_ctx, controller->context_size, (x_epno));
        int val = (2 << 3) | (3 << 1) | (max_packet << 16);
        ep_ctx->field2 = val;

        ep->seg = new_seg;
        ep->x_epno = x_epno;

        ep_ctx->deq_addr = ep->seg.trbs_dma | ep->seg.cycle_state;
        ep_ctx->field4 = (8);
        ctrl->add_flags = (BIT(x_epno) + 1) | 0x1;
        ctrl->drop_flags = 0;
        xhci_configure_ep(controller, xdev->slot_id, xdev->in_ctx.dma_addr);

        xdev->ep_segs[x_epno] = &ep->seg;
        return x_epno;
    } else {
        ctrl = xhci_get_control_ctx(&xdev->in_ctx);
        ep_ctx =
            xhci_get_ep_ctx(&xdev->in_ctx, controller->context_size, (x_epno));
        int val = (6 << 3) | (3 << 1) | (max_packet << 16);
        ep_ctx->field2 = val;

        ep->seg = new_seg;
        ep->x_epno = x_epno;

        ep_ctx->deq_addr = ep->seg.trbs_dma | ep->seg.cycle_state;
        ep_ctx->field4 = (8);
        ctrl->add_flags = (BIT(x_epno) + 1) | 0x1;
        ctrl->drop_flags = 0;
        xhci_configure_ep(controller, xdev->slot_id, xdev->in_ctx.dma_addr);

        xdev->ep_segs[x_epno] = &ep->seg;
        return x_epno;
    }
}

/*
 * Parse the supported protocol extended capability
 */
void xhci_get_port_speeds(struct xhci_hcd *controller) {
    uint32_t cparams;
    cparams = controller->cap_regs->hccparams1;

    size_t eoff = (((cparams & 0xFFFF0000) >> 16) * 4);
    volatile uint32_t *extcap =
        (uint32_t *)((size_t)controller->cap_regs + eoff);
    if (!extcap) {
        return;
    }

    while (1) {
        uint32_t val = (uint32_t)*extcap;
        if (val == 0xFFFFFFFF) {
            break;
        }
        if (!(val & 0xFF)) {
            break;
        }

        kprint(KPRN_INFO, "usb/xhci found extcap: %X %X", val & 0xFF, extcap);
        if ((val & 0xff) == 2) {
            uint32_t *old_ext = (uint32_t *)extcap;

            struct xhci_port_protocol protocol = {0};
            uint32_t value = *extcap;
            protocol.major = (value >> 24) & 0xFF;
            protocol.minor = (value >> 16) & 0xFF;
            extcap++;
            value = *extcap;
            protocol.name[0] = (char)(value & 0xFF);
            protocol.name[1] = (char)((value >> 8) & 0xFF);
            protocol.name[2] = (char)((value >> 16) & 0xFF);
            protocol.name[3] = (char)((value >> 24) & 0xFF);
            protocol.name[4] = '\0';
            extcap++;
            value = *extcap;
            protocol.compatible_port_off = value & 0xFF;
            protocol.compatible_port_count = (value >> 8) & 0xFF;
            protocol.protocol_specific = (value >> 16) & 0xFF;
            int speed_id_cnt = (value >> 28) & 0xF;
            extcap++;
            value = *extcap;
            protocol.protocol_slot_type = value & 0xF;
            extcap++;

            for (int i = 0; i < speed_id_cnt; i++) {
                value = *extcap;

                protocol.speeds[i].value = value & 0xF;
                protocol.speeds[i].exponent = (value >> 4) & 0x3;
                protocol.speeds[i].type = (value >> 6) & 0x3;
                protocol.speeds[i].full_duplex = (value >> 8) & 0x1;
                protocol.speeds[i].link_protocol = (value >> 14) & 0x3;
                protocol.speeds[i].mantissa = (value >> 16) & 0xFFFF;

                extcap++;
            }

            kprint(KPRN_INFO, "usb/xhci: port speed capability");
            kprint(KPRN_INFO,
                   "usb/xhci: protocol version: %u.%u, name: %s, offset: %X, "
                   "count = %X",
                   protocol.major, protocol.minor, protocol.name,
                   protocol.compatible_port_off,
                   protocol.compatible_port_count);

            extcap = old_ext;
            controller->protocols[controller->num_protcols] = protocol;
            controller->num_protcols++;
        }

        uint32_t *old = (uint32_t *)extcap;
        extcap = (uint32_t *)((size_t)extcap + (((val >> 8) & 0xFF) << 2));
        if (old == extcap) {
            break;
        }
    }
}

/*
 * Take the controller's ownership from the BIOS
 */
void xhci_take_controller(struct xhci_hcd *controller) {
    uint32_t cparams;

    cparams = controller->cap_regs->hccparams1;

    size_t eoff = (((cparams & 0xFFFF0000) >> 16) * 4);
    volatile uint32_t *extcap =
        (uint32_t *)((size_t)controller->cap_regs + eoff);
    if (!extcap) {
        return;
    }

    // find the legacy capability
    while (1) {
        uint32_t val = *extcap;
        if (val == 0xFFFFFFFF) {
            break;
        }
        if (!(val & 0xFF)) {
            break;
        }

        kprint(KPRN_INFO, "usb/xhci found extcap: %X %X", val & 0xFF, extcap);
        if ((val & 0xff) == 1) {
            // Bios semaphore
            volatile uint8_t *bios_sem = (uint8_t *)((size_t)(extcap) + 0x2);
            if (*bios_sem) {
                kprint(KPRN_INFO, "usb/xhci: device is bios-owned");
                volatile uint8_t *os_sem = (uint8_t *)((size_t)(extcap) + 0x3);
                *os_sem = 1;
                while (1) {
                    bios_sem = (uint8_t *)((size_t)(extcap) + 0x2);
                    if (*bios_sem == 0) {
                        kprint(KPRN_INFO,
                               "usb/xhci: device is no longer bios-owned");
                        break;
                    }
                    ksleep(100);
                }
            }
        }

        uint32_t *old = (uint32_t *)extcap;
        extcap = (uint32_t *)((size_t)extcap + (((val >> 8) & 0xFF) << 2));
        if (old == extcap) {
            break;
        }
    }
}

static void xhci_detect_devices(struct xhci_hcd *controller) {
    xhci_get_port_speeds(controller);
    uint32_t hccparams = controller->cap_regs->hcsparams1;
    for (int i = 0; i < controller->num_protcols; i++) {
        for (int j = controller->protocols[i].compatible_port_off;
             (j - controller->protocols[i].compatible_port_off) <
             controller->protocols[i].compatible_port_count;
             j++) {
            volatile struct xhci_port_regs *port_regs1 =
                &controller->op_regs->prs[j - 1];
            // try to power the port if it's off
            if (!(port_regs1->portsc & (1 << 9))) {
                port_regs1->portsc = 1 << 9;
                ksleep(20);
                if (!(port_regs1->portsc & (1 << 9))) {
                    // TODO use interrupt here
                    continue;
                }
            }

            // clear status change bits
            port_regs1->portsc = (1 << 9) | ((1 << 17) | (1 << 18) | (1 << 20) |
                                             (1 << 21) | (1 << 22));

            // usb 2 devices have bit 4 as reset, usb 3 has 31
            if (controller->protocols[i].major == 2) {
                port_regs1->portsc = (1 << 9) | (1 << 4);
            } else {
                port_regs1->portsc = (1 << 9) | (1 << 31);
            }

            int timeout = 0;
            int reset = 0;
            while (1) {
                if (port_regs1->portsc & (1 << 21)) {
                    reset = 1;
                    break;
                }
                if (timeout++ == 500) {
                    break;
                }
                ksleep(1);
            }
            if (!reset) {
                continue;
            }
            // apparently this delay is necessary
            ksleep(3);
            if (port_regs1->portsc & (1 << 1)) {
                port_regs1->portsc =
                    (1 << 9) |
                    ((1 << 17) | (1 << 18) | (1 << 20) | (1 << 21) | (1 << 22));
                xhci_init_device(controller, j - 1, controller->protocols[i]);
            }
        }
    }
}

static int xhci_send_bulk(struct usb_dev_t *dev, char *data, size_t size,
                          int epno, int out) {
    (void)out;
    struct xhci_hcd *controller = dev->internal_controller;
    struct xhci_dev *xdev = controller->xdevs[dev->hcd_devno];
    struct xhci_seg *seg;
    volatile struct xhci_db_regs *dbr;
    uint32_t slot_id;

    /*
     * TODO split transfers that cross 64KB boundaries
     */

    slot_id = xdev->slot_id;
    seg = xdev->ep_segs[epno];
    spinlock_acquire(&seg->lock);
    dbr = controller->db_regs;
    struct xhci_transfer_trb *trb = xhci_get_trb(seg);

    fill_trb_buf((struct xhci_command_trb *)trb,
                 (uint32_t)(((size_t)data - MEM_PHYS_OFFSET) & 0xFFFFFFFF),
                 (uint32_t)(((size_t)data - MEM_PHYS_OFFSET) >> 32), size,
                 TRB_CMD_TYPE(TRB_NORMAL) | (1 << 2) | (1 << 5) | (0 << 4), 0);

    uint32_t index = (seg->enq - (uint64_t)seg->trbs) / 0x10;
    struct xhci_event ev = {0};
    seg->seg_events[index] = &ev;
    dbr->db[slot_id] = epno;
    event_await(&ev.event);
    trb->addr = 0;
    trb->len = 0;
    trb->flags = 0;
    spinlock_release(&seg->lock);
    return (ev.trb.status >> 24) != 1;
}

void xhci_irq_handler(struct xhci_hcd *controller) {
    for (;;) {
        event_await(&int_event[controller->irq_line]);
        // handle events
        volatile struct xhci_event_trb *event =
            (struct xhci_event_trb *)controller->ering.deq;
        size_t deq = 0;
        while ((event->flags & 0x1) == controller->ering.cycle_state) {
            if (DEBUG_EVENTS) {
                kprint(KPRN_INFO, "usb/xhci: event deq: %X", event);
                kprint(KPRN_INFO, "usb/xhci: TRB type: %X",
                       TRB_TYPE(event->flags));
                kprint(KPRN_INFO, "usb/xhci: TRB status: %X", (event->status));
            }

            switch (TRB_TYPE(event->flags)) {
                case 33: {
                    size_t index =
                        (event->addr -
                         ((size_t)controller->crseg.trbs - MEM_PHYS_OFFSET)) /
                        0x10;
                    if (DEBUG_EVENTS) {
                        kprint(KPRN_INFO, "usb/xhci: Got command completion");
                        kprint(KPRN_INFO,
                               "usb/xhci: dequeueing event at index %X", index);
                    }
                    if (controller->crseg.seg_events[index]) {
                        controller->crseg.seg_events[index]->trb = *event;
                        event_trigger(
                            &controller->crseg.seg_events[index]->event);
                        controller->crseg.seg_events[index] = NULL;
                    }
                    break;
                }
                case 0x20: {
                    if (DEBUG_EVENTS) {
                        kprint(KPRN_INFO, "usb/xhci: Got transfer completion");
                    }
                    int slot_id = (event->flags >> 24) & 0xFF;
                    int epid = (event->flags >> 16) & 0x1F;
                    struct xhci_seg *tseg = NULL;
                    if (epid == 1) {
                        tseg = &controller->xdevs[slot_id]->control;
                    } else {
                        tseg = controller->xdevs[slot_id]->ep_segs[epid];
                    }
                    size_t index =
                        (event->addr - ((size_t)tseg->trbs - MEM_PHYS_OFFSET)) /
                        0x10;
                    if (index == 0xfe) {
                        index = -1;
                    }
                    if (tseg->seg_events[index + 1]) {
                        tseg->seg_events[index + 1]->trb = *event;
                        event_trigger(&tseg->seg_events[index + 1]->event);
                        tseg->seg_events[index + 1] = NULL;
                    }
                    break;
                }
                case 34: {
                    kprint(KPRN_INFO, "usb/xhci: Port %X changed state",
                           (event->addr));
                    if ((event->addr >> 24) < XHCI_CONFIG_MAX_SLOT) {
                        event_trigger(
                            &controller->port_events[event->addr >> 24]);
                    }
                    break;
                }
            }

            controller->ering.deq = (uint64_t)(event + 1);
            size_t index =
                controller->ering.deq - (uint64_t)controller->ering.trbs;
            uint64_t val = (size_t)controller->ering.trbs - MEM_PHYS_OFFSET;
            val += (index % 4096);
            deq = val;
            if (!(index % 4096)) {
                controller->ering.deq = (uint64_t)controller->ering.trbs;
                controller->ering.cycle_state = !controller->ering.cycle_state;
            }

            event = (struct xhci_event_trb *)controller->ering.deq;
        }

        controller->run_regs->irs[0].iman =
            controller->run_regs->irs[0].iman | 1;
        controller->run_regs->irs[0].erdp = deq | (1 << 3);
    }
}

/*
 * Switch ports from the ehci controller to the xhci controller
 * this is needed on some intel xhci controllers.
 *
 * TODO: apparently on some specific devices this may not work
 * and the ports can't be switched to the xhci controller.
 */
void xhci_switch_ports(struct pci_device_t *pci_dev) {
    uint32_t usb3_enable = pci_read_device_dword(pci_dev, 0xDC);
    kprint(KPRN_INFO, "usb/xhci: ports that can be switched to xhci: %X",
           usb3_enable);
    pci_write_device_dword(pci_dev, 0xD8, usb3_enable);

    uint32_t usb2_enable = pci_read_device_dword(pci_dev, 0xD4);
    pci_write_device_dword(pci_dev, 0xD0, usb2_enable);

    uint32_t switched = pci_read_device_dword(pci_dev, 0xD0);
    kprint(KPRN_INFO, "usb/xhci: ports that have been switched to xhci: %X",
           switched);
}

void xhci_controller_init(struct pci_device_t *pci_dev) {
    struct pci_bar_t bar = {0};

    panic_if(pci_read_bar(pci_dev, 0, &bar));
    panic_unless(bar.is_mmio);

    panic_unless((pci_read_device_dword(pci_dev, 0x10) & 0b111) == 0b100);

    struct xhci_hcd *controller = kalloc(sizeof(struct xhci_hcd));
    size_t base = bar.base + MEM_PHYS_OFFSET;
    controller->cap_regs = (struct xhci_cap_regs *)(base);
    controller->op_regs =
        (struct xhci_op_regs *)(base + (controller->cap_regs->caplength));
    controller->run_regs =
        (struct xhci_run_regs *)(base + (controller->cap_regs->rtsoff));
    controller->db_regs =
        (struct xhci_db_regs *)(base + (controller->cap_regs->dboff));

    // Allocate as many devices as there are slots available.
    controller->xdevs = kalloc(sizeof(struct xhci_dev) *
                               (controller->cap_regs->hcsparams1 & 0xFF));

    xhci_take_controller(controller);

    // TODO do this only on controller which require it
    xhci_switch_ports(pci_dev);

    // Set up irqs
    controller->irq_line = get_empty_int_vector();
    if (!pci_register_msi(pci_dev, controller->irq_line)) {
        io_apic_connect_gsi_to_vec(0, controller->irq_line, pci_dev->gsi,
                                   pci_dev->gsi_flags, 1);
    }

    int32_t cmd = controller->op_regs->usbcmd;
    controller->op_regs->usbcmd = cmd | 1 << 1;
    // Wait for controller not ready
    // intel xhci controllers need this delay
    ksleep(100);
    while ((controller->op_regs->usbcmd & (1 << 1))) {
    };
    while (!(controller->op_regs->usbsts & 0x00000001UL)) {
    };
    kprint(KPRN_INFO, "usb/xhci: controller halted");

    controller->op_regs->config = XHCI_CONFIG_MAX_SLOT;
    uint32_t hccparams = controller->cap_regs->hccparams1;
    if (hccparams & 0b10) {
        controller->context_size = 64;
    } else {
        controller->context_size = 32;
    }

    controller->dcbaap = kalloc(2048);
    controller->dcbaap_dma = (size_t)controller->dcbaap - MEM_PHYS_OFFSET;
    controller->op_regs->dcbaap = controller->dcbaap_dma;

    // Set up scratchpad_buffers
    uint32_t hcs2 = controller->cap_regs->hcsparams2;
    int spb = ((((hcs2) >> 16) & 0x3e0) | (((hcs2) >> 27) & 0x1f));
    if (spb) {
        controller->scratchpad_buffer_array = kalloc(sizeof(uint64_t) * spb);
        kprint(KPRN_INFO, "usb/xhci: allocating %x scratchpad_buffers", spb);
        for (int i = 0; i < spb; i++) {
            size_t scratchpad_buffer = (size_t)kalloc(PAGE_SIZE);
            controller->scratchpad_buffer_array[i] =
                scratchpad_buffer - MEM_PHYS_OFFSET;
        }
    }
    // the first entry in the dcbapp has to point to the buffers if they exist
    controller->dcbaap[0] =
        (size_t)controller->scratchpad_buffer_array - MEM_PHYS_OFFSET;
    xhci_setup_seg(&controller->crseg, 4096, TYPE_COMMAND);
    controller->op_regs->crcr = controller->crseg.trbs_dma | 1;
    kprint(KPRN_INFO, "usb/xhci: Initializing event ring");
    xhci_setup_seg(&controller->ering, 4096, TYPE_EVENT);

    // Set up event ring segment table.
    controller->erst.entries = kalloc(4096);
    controller->erst.dma = (size_t)controller->erst.entries - MEM_PHYS_OFFSET;
    controller->erst.num_segs = 1;
    controller->erst.entries->addr = controller->ering.trbs_dma;
    controller->erst.entries->size = controller->ering.size;
    controller->erst.entries->reserved = 0;

// We only need one interrupter since we either use msi
// or pin-based irqs.
// Set the event ring dequeue pointer
#define XHCI_ERDP_MASK (~(0xFUL))
    uint64_t erdp = controller->run_regs->irs[0].erdp & ~XHCI_ERDP_MASK;
    erdp = erdp | (controller->ering.trbs_dma & XHCI_ERDP_MASK);
    controller->run_regs->irs[0].erdp = erdp;
    // enable interrupts for this interrupter
    controller->run_regs->irs[0].iman = 1 << 1;

// Set up event ring size
#define XHCI_ERST_SIZE_MASK 0xFFFF
    uint32_t erstsz =
        controller->run_regs->irs[0].erstsz & ~XHCI_ERST_SIZE_MASK;
    erstsz = erstsz | controller->erst.num_segs;
    controller->run_regs->irs[0].erstsz = erstsz;

// Set the base address for the event ring segment table
#define XHCI_ERST_ADDR_MASK (~(0x3FUL))
    uint64_t erstba =
        controller->run_regs->irs[0].erstba & ~XHCI_ERST_ADDR_MASK;
    erstba = erstba | (controller->erst.dma & XHCI_ERST_ADDR_MASK);
    controller->run_regs->irs[0].erstba = erstba;

    // Start controller again
    cmd = controller->op_regs->usbcmd;
    cmd |= 0x1 | 1 << 2;
    controller->op_regs->usbcmd = cmd;

    task_tcreate(0, tcreate_fn_call,
                 tcreate_fn_call_data(0, &xhci_irq_handler, controller));

    // Wait for the controller to restart
    while ((controller->op_regs->usbsts & 0x1)) {
    };
    kprint(KPRN_INFO, "usb/xhci: controller restarted");
    xhci_detect_devices(controller);
}

struct usb_hc_t *usb_init_xhci(void) {
    xhci_controller = kalloc(sizeof(struct usb_hc_t));
    xhci_controller->send_control = xhci_send_control;
    xhci_controller->send_bulk = xhci_send_bulk;
    xhci_controller->setup_endpoint = xhci_setup_endpoint;

    int i = 0;
    while (1) {
        struct pci_device_t *pci_dev =
            pci_get_device(XHCI_CLASS, XHCI_SUBCLASS, PROG_IF, i);
        pci_enable_busmastering(pci_dev);
        if (!pci_dev) {
            break;
        }
        kprint(KPRN_INFO, "usb/xhci: found xhci controller!");
        xhci_controller_init(pci_dev);
        i++;
    }
    return xhci_controller;
}
