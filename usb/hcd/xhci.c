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
    kprint(KPRN_INFO, "setting up seg: %X", addr, seg->trbs_dma);
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
        kprint(KPRN_INFO, "usb/xhci: CYCLE");
        link->addr = seg->trbs_dma;
        link->field2 = 0;
        link->field3 = (0x1 | TRB_CMD_TYPE(TRB_LINK));
    }
    return 1;
}

static void *xhci_get_trb(volatile struct xhci_seg *seg) {
    uint64_t val, enq;
    uint32_t index;
    struct xhci_link_trb *link;

    enq = val = seg->enq;
    val = val + 16;
    index = (enq - (uint64_t)seg->trbs) / 16 + 1;
    /* TRBs being a cyclic buffer, here we cycle back to beginning. */
    if (index == (seg->size - 1)) {
        kprint(KPRN_INFO, "usb/xhci: CYCLE");
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

/*
 * first determines whether this trb is the first in a transfer,
 * since in this case the cycle bit should not be toggled to prevent
 * the controller from getting a partially formed chain.
 */
static void xhci_enqueue_trb(struct xhci_seg *seg, void *trb,
                             struct xhci_event *ev, int first) {

    uint32_t index = (seg->enq - (uint64_t)seg->trbs) / 0x10;
    struct xhci_command_trb *command = xhci_get_trb(seg);
    memcpy(command, trb, sizeof(struct xhci_command_trb));
    kprint(KPRN_INFO, "usb/xhci: enqueuing comannd at %X", index);
    seg->seg_events[index] = ev;

    if (!first) {
        uint32_t val = command->field[3];
        uint32_t cycle_state = (val & 0x1) ? 0 : 1;
        val = command->field[3] | cycle_state;
        command->field[3] = val;
    }
}

static int xhci_send_command(struct xhci_hcd *controller, uint32_t field1,
                             uint32_t field2, uint32_t field3, uint32_t field4,
                             struct xhci_event *ev) {

    volatile struct xhci_db_regs *dbr;
    volatile struct xhci_command_trb *cmd;

    dbr = controller->db_regs;
    cmd = (struct xhci_command_trb *)controller->crseg.enq;

    struct xhci_command_trb trb = {0};
    trb.field[0] = field1;
    trb.field[1] = field2;
    trb.field[2] = field3;
    trb.field[3] = field4;
    xhci_enqueue_trb(&controller->crseg, (void *)&trb, ev, 0);

    dbr->db[0] = 0;

    cmd++;
    controller->crseg.enq = (uint64_t)cmd;
    return 0;
}

void xhci_enable_slot(struct xhci_hcd *controller) {
    uint32_t field1 = 0, field2 = 0, field3 = 0,
             field4 = TRB_CMD_TYPE(TRB_ENABLE_SLOT);
    struct xhci_event ev = {0};
    xhci_send_command(controller, field1, field2, field3, field4, &ev);
    event_await(&ev.event);
    if (TRB_STATUS(ev.trb.status)) {
        int slot = TRB_SLOT_ID(ev.trb.flags);
        kprint(KPRN_INFO, "usb/xhci: slot %x has been enabled", slot);
        controller->slot_id = slot;
    } else {
        kprint(KPRN_INFO, "usb/xhci: failed to enable slot");
        controller->slot_id = 0;
    }
}

static void xhci_send_addr_device(struct xhci_hcd *xhcd, uint32_t slot_id,
                                  uint64_t dma_in_ctx, int bsr) {
    uint32_t field1, field2, field3, field4;

    field1 = TRB_ADDR_LOW(dma_in_ctx) & ~0xF;
    field2 = TRB_ADDR_HIGH(dma_in_ctx);
    field3 = 1;
    field4 =
        TRB_CMD_TYPE(TRB_ADDRESS_DEV) | TRB_CMD_SLOT_ID(slot_id) | (bsr << 9);
    struct xhci_event ev = {0};
    xhci_send_command(xhcd, field1, field2, field3, field4, &ev);
    event_await(&ev.event);
    if ((ev.trb.status >> 24) != 1) {
        kprint(KPRN_INFO,
               "usb/xhci: Error while attempting to address device: %X",
               ev.trb.status);
    } else {
        kprint(KPRN_INFO, "usb/xhci: Device addressed");
    }
}

static void xhci_configure_ep(struct xhci_hcd *xhcd, uint32_t slot_id,
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
    } else {
        kprint(KPRN_INFO, "usb/xhci: Endpoint configured");
    }
}

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

int xhci_init_device(struct xhci_hcd *controller, uint32_t port,
                     uint32_t speed) {
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

    xhci_setup_seg(&xhci_dev->control, 4096, TYPE_CTRL);

    xhci_setup_seg(&xhci_dev->control, 4096, TYPE_CTRL);

    ep0 = xhci_get_ep0_ctx(&xhci_dev->in_ctx, controller->context_size);
    if (speed == 3) {
        max_packet = 512;
    } // TODO support usb 2 and 1 fs/ls
    uint32_t val = EP_TYPE(EP_CTRL) | MAX_BURST(0) | ERROR_COUNT(3) |
                   MAX_PACKET_SIZE(max_packet);
    ep0->field2 = val;
    ep0->deq_addr =
        (xhci_dev->control.trbs_dma | xhci_dev->control.cycle_state);

    xhci_setup_context(&xhci_dev->out_ctx, 4096, XHCI_CTX_TYPE_DEVICE);
    controller->dcbaap[slot_id] = xhci_dev->out_ctx.dma_addr;
    slot = xhci_get_slot_ctx(&xhci_dev->out_ctx, controller->context_size);
    kprint(KPRN_INFO, "usb/xhci: sending address device %x %x",
           xhci_dev->in_ctx.dma_addr, (size_t)slot);
    ksleep(10);
    xhci_send_addr_device(controller, slot_id, xhci_dev->in_ctx.dma_addr, 0);
    ctrl->add_flags = (0x1);
    xhci_configure_ep(controller, slot_id, xhci_dev->in_ctx.dma_addr);
    xhci_dev->ep_segs[1] = &xhci_dev->control;

    struct usb_dev_t udevice = {0};
    udevice.max_packet_size = 512;
    udevice.controller = xhci_controller;
    xhci_dev->xhci_controller = controller;
    controller->xdevs[slot_id] = xhci_dev;
    xhci_dev->slot_id = slot_id;
    udevice.hcd_dev = xhci_dev;
    if (usb_add_device(udevice))
        return -1;
    return 0;
}

static struct xhci_ep_ctx *xhci_get_ep_ctx(volatile struct xhci_ctx *ctx,
                                           uint32_t ctx_size, uint32_t epno) {

    if (ctx->type == XHCI_CTX_TYPE_INPUT)
        epno++;
    return (struct xhci_ep_ctx *)(ctx->addr + epno * ctx_size);
}

int xhci_setup_endpoint(struct usb_dev_t *dev, int epno) {
    struct xhci_endpoint *ep = kalloc(sizeof(struct xhci_endpoint));
    ep->lock = new_lock;
    struct xhci_dev *xdev = dev->hcd_dev;
    struct xhci_hcd *controller = xdev->xhci_controller;
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
    int endpoint_no = dev->endpoints[epno].data.address & 0x0F;
    kprint(KPRN_INFO, "Endpoint address: %x",
           dev->endpoints[epno].data.address);
    if (dev->endpoints[epno].data.address & 0x80) {
        kprint(KPRN_INFO, "Endpoint is input %x");
        out = 0;
    }

    // TODO this is a hack to fix a bug in the USB code
    if (dev->endpoints[epno].data.address == 0) {
        out = 1;
        endpoint_no = 2;
    }

    struct xhci_seg new_seg;
    xhci_setup_seg(&new_seg, 4096, TYPE_BULK);

    int x_epno = xhci_get_epno(endpoint_no, !out);

    in_slot->field1 &= ~((0x1f << 27));
    in_slot->field1 |= ((x_epno + 1) << 27);

    kprint(KPRN_INFO, "usb/xhci: setting up endpoint %x, ep number: %x",
           endpoint_no, x_epno);
    if (out) { // bulk out
        kprint(KPRN_INFO, "usb/xhci: endpoint %x is a bulk endpoint", epno);

        ctrl = xhci_get_control_ctx(&xdev->in_ctx);
        ep_ctx =
            xhci_get_ep_ctx(&xdev->in_ctx, controller->context_size, (x_epno));
        int val = (2 << 3) | (3 << 1) |
                  (dev->endpoints[epno].data.max_packet_size << 16);
        ep_ctx->field2 = val;

        ep->seg = new_seg;
        ep->x_epno = x_epno;

        ep_ctx->deq_addr = ep->seg.trbs_dma | ep->seg.cycle_state;
        ep_ctx->field4 = (8);
        ctrl->add_flags = (BIT(x_epno) + 1) | 0x1;
        ctrl->drop_flags = 0;
        xhci_configure_ep(controller, xdev->slot_id, xdev->in_ctx.dma_addr);
        dev->endpoints[epno].hcd_ep = ep;

        xdev->ep_segs[x_epno] = &ep->seg;
        return 0;
    } else { // bulk in
        kprint(KPRN_INFO, "usb/xhci: endpoint %x is a bulk endpoint", epno);

        ctrl = xhci_get_control_ctx(&xdev->in_ctx);
        ep_ctx =
            xhci_get_ep_ctx(&xdev->in_ctx, controller->context_size, (x_epno));
        int val = (6 << 3) | (3 << 1) |
                  (dev->endpoints[epno].data.max_packet_size << 16);
        ep_ctx->field2 = val;

        ep->seg = new_seg;
        ep->x_epno = x_epno;
        kprint(KPRN_INFO, "usb/xhci: seg dma ctx %X", (size_t)ep->seg.trbs_dma);

        ep_ctx->deq_addr = ep->seg.trbs_dma | ep->seg.cycle_state;
        ep_ctx->field4 = (8);
        ctrl->add_flags = (BIT(x_epno) + 1) | 0x1;
        ctrl->drop_flags = 0;
        xhci_configure_ep(controller, xdev->slot_id, xdev->in_ctx.dma_addr);
        dev->endpoints[epno].hcd_ep = ep;

        xdev->ep_segs[x_epno] = &ep->seg;
        return 0;
    }
}

static void xhci_detect_devices(struct xhci_hcd *controller) {
    ksleep(100);
    uint32_t hccparams = controller->cap_regs->hcsparams1;
    uint32_t max_ports = hccparams >> 24;
    // TODO this only works for usb 3 devices for now.
    for (int i = 0; i < max_ports; i++) {
        volatile struct xhci_port_regs *port_regs1 =
            &controller->op_regs->prs[i];
        if (port_regs1->portsc & (1 << 9)) {
            kprint(KPRN_INFO, "usb/xhci: device on port %X, portsc: %X", i,
                   port_regs1->portsc);
            // reset all change bits and reset the device
            port_regs1->portsc = (1 << 9) | (1 << 21) | (1 << 20) | (1 << 19) |
                                 (1 << 18) | (1 << 17) | (1 << 22) | (1 << 23) |
                                 (1 << 4);
            kprint(KPRN_INFO, "usb/xhci: waiting for enable on port");
            int timeout = 0;
            // TODO use irqs for this
            while (!(port_regs1->portsc & (1 << 1))) {
                if (timeout++ == 100000) {
                    break;
                }
            }
            kprint(KPRN_INFO, "usb/xhci: portsc: %X", port_regs1->portsc);
            if (!(port_regs1->portsc & 0x1)) {
                continue;
            }
            while (1) {
                // on some devices the port seems to be in a state that is
                // incompatible with the state diagram so wait for it to fully
                // reset itself.
                if ((((port_regs1->portsc >> 5) & 0xf) == 0)) {
                    // The port can be initialized.
                    xhci_init_device(controller, i, 3);
                    break;
                }
            }
        }
    }
}

static void fill_trb_buff(volatile struct xhci_command_trb *cmd,
                          uint32_t field1, uint32_t field2, uint32_t field3,
                          uint32_t field4) {
    uint32_t val, cycle_state;

    cmd->field[0] = (field1);
    cmd->field[1] = (field2);
    cmd->field[2] = (field3);

    val = (cmd->field[3]);
    cycle_state = (val & 0x1) ? 0 : 1;
    val = cycle_state | (field4 & ~0x1);
    cmd->field[3] = (val);

    return;
}

static void fill_setup_data(struct xhci_command_trb *cmd, void *data,
                            uint32_t size, uint32_t dir) {
    uint32_t field1, field2, field3, field4;

    field1 = TRB_ADDR_LOW(data);
    field2 = TRB_ADDR_HIGH(data);
    field3 = size;
    field4 = TRB_CMD_TYPE(TRB_DATA_STAGE);
    if (dir)
        field4 |= TRB_DIR_IN;
    fill_trb_buff(cmd, field1, field2, field3, field4);
}

static void fill_setup_trb(struct xhci_command_trb *cmd,
                           struct usb_request_t *req, uint32_t size, int pid) {
    (void)size;
    uint32_t field1, field2, field3, field4 = 0;
    uint64_t req_raw;

    req_raw = *((uint64_t *)req);
    field2 = (TRB_ADDR_HIGH(req_raw));
    field1 = (TRB_ADDR_LOW(req_raw));
    field3 = 8;

    if (pid) {
        field4 = TRB_TRT(3);
    }
    field4 |= TRB_CMD_TYPE(TRB_SETUP_STAGE) | TRB_IDT;
    fill_trb_buff(cmd, field1, field2, field3, field4);
}

static void fill_status_trb(struct xhci_command_trb *cmd, uint32_t dir) {
    uint32_t field1, field2, field3, field4;

    field1 = 0;
    field2 = 0;
    field3 = 0;
    field4 = TRB_CMD_TYPE(TRB_STATUS_STAGE) | TRB_IOC;
    if (dir)
        field4 |= TRB_DIR_IN;
    fill_trb_buff(cmd, field1, field2, field3, field4);
}

static void fill_normal_trb(struct xhci_transfer_trb *trb, void *data,
                            uint32_t size, int is_last) {
    uint32_t field1, field2, field3, field4;

    field1 = (uint32_t)(((size_t)data) & 0xFFFFFFFF);
    field2 = (uint32_t)(((size_t)data) >> 32);
    field3 = size;
    field4 =
        TRB_CMD_TYPE(TRB_NORMAL) | (1 << 2) | (is_last << 5) | (!is_last << 4);
    fill_trb_buff((struct xhci_command_trb *)trb, field1, field2, field3,
                  field4);
}

static int xhci_send_control(struct usb_dev_t *device, char *data, size_t size,
                             int endpoint, int out) {
    (void)out;
    (void)endpoint;

    struct xhci_dev *dev = device->hcd_dev;
    struct xhci_hcd *controller = dev->xhci_controller;
    struct xhci_seg *ctrl;
    struct xhci_command_trb *cmd;
    volatile struct xhci_db_regs *dbr;
    struct usb_request_t *req = (struct usb_request_t *)data;
    uint32_t slot_id, pid = 0;
    slot_id = dev->slot_id;
    ctrl = &dev->control;
    dbr = controller->db_regs;

    cmd = xhci_get_trb(ctrl);
    fill_setup_trb(cmd, req, size, 1);
    data += sizeof(struct usb_request_t);

    cmd = xhci_get_trb(ctrl);

    if (req->length) {
        pid = 1;
        fill_setup_data(cmd, (void *)((size_t)data - MEM_PHYS_OFFSET),
                        req->length, pid);
        cmd = xhci_get_trb(ctrl);
    }
    fill_status_trb(cmd, pid);
    uint32_t index = (ctrl->enq - (uint64_t)ctrl->trbs) / 0x10;
    kprint(KPRN_INFO, "usb/xhci: control event index is at %X", index);
    struct xhci_event ev = {0};
    ;
    ctrl->seg_events[index] = &ev;
    dbr->db[slot_id] = 1;
    event_await(&ev.event);
    return 0;
}

static int xhci_send_bulk(struct usb_dev_t *dev, char *data, size_t size,
                          int epno, int out) {
    kprint(KPRN_INFO, "BULK OPERATION");
    (void)out;
    struct xhci_dev *xdev = dev->hcd_dev;
    struct xhci_hcd *controller = xdev->xhci_controller;
    struct xhci_seg *seg;
    struct xhci_transfer_trb *trb;
    volatile struct xhci_db_regs *dbr;
    struct xhci_endpoint *xep =
        ((struct xhci_endpoint *)dev->endpoints[epno].hcd_ep);
    spinlock_acquire(&xep->lock);
    uint32_t slot_id;

    slot_id = xdev->slot_id;
    seg = &xep->seg;
    dbr = controller->db_regs;
    trb = xhci_get_trb(seg);
    fill_normal_trb(trb, (void *)((size_t)data - MEM_PHYS_OFFSET), size, 1);
    uint32_t index = (seg->enq - (uint64_t)seg->trbs) / 0x10;
    kprint(KPRN_INFO, "usb/xhci: control event index is at %X, enq:%X trbs:%X",
           index, seg->enq, seg->trbs);
    struct xhci_event ev = {0};
    ;
    seg->seg_events[index] = &ev;
    dbr->db[slot_id] = xep->x_epno;
    event_await(&ev.event);
    trb->addr = 0;
    trb->len = 0;
    trb->flags = 0;
    spinlock_release(&xep->lock);
    return 0;
}

void xhci_irq_handler(struct xhci_hcd *controller) {
    kprint(KPRN_INFO, "IRQ worker started");
    for (;;) {
        event_await(&int_event[controller->irq_line]);

        kprint(KPRN_INFO, "GOT IRQ");

        // handle events
        volatile struct xhci_event_trb *event =
            (struct xhci_event_trb *)controller->ering.deq;
        kprint(KPRN_INFO, "usb/xhci: event deq: %X", event);
        size_t deq = 0;
        while ((event->flags & 0x1) == controller->ering.cycle_state) {
            kprint(KPRN_INFO, "usb/xhci: TRB type: %X", TRB_TYPE(event->flags));
            kprint(KPRN_INFO, "usb/xhci: TRB status: %X", (event->status));

            switch (TRB_TYPE(event->flags)) {
                case 33: {
                    kprint(KPRN_INFO, "usb/xhci: Got command completion");
                    int index = (event->addr - ((size_t)controller->crseg.trbs -
                                                MEM_PHYS_OFFSET)) /
                                0x10;
                    kprint(KPRN_INFO, "usb/xhci: dequeueing event at index %X",
                           index);
                    if (controller->crseg.seg_events[index]) {
                        controller->crseg.seg_events[index]->trb = *event;
                        event_trigger(
                            &controller->crseg.seg_events[index]->event);
                        controller->crseg.seg_events[index] = NULL;
                    }
                    break;
                }
                case 0x20: {
                    kprint(KPRN_INFO, "usb/xhci: Got transfer completion");
                    int slot_id = (event->flags >> 24) & 0xFF;
                    int epid = (event->flags >> 16) & 0x1F;
                    struct xhci_seg *tseg =
                        controller->xdevs[slot_id]->ep_segs[epid];
                    int index =
                        (event->addr - ((size_t)tseg->trbs - MEM_PHYS_OFFSET)) /
                        0x10;
                    if (index == 0xfe) {
                        index = -1;
                    }
                    kprint(KPRN_INFO,
                           "usb/xhci: dequeueing event at index %X for %X %X "
                           "at %X, %X",
                           index + 1, slot_id, epid, tseg->trbs, event->addr);
                    if (tseg->seg_events[index + 1]) {
                        tseg->seg_events[index + 1]->trb = *event;
                        event_trigger(&tseg->seg_events[index + 1]->event);
                        tseg->seg_events[index + 1] = NULL;
                    }
                }
                case 34: {
                    kprint(KPRN_INFO, "usb/xhci: Port %X changed state",
                           (event->addr));
                }
            }

            controller->ering.deq = (uint64_t)(event + 1);
            int index =
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
 * Take the controller's ownership from the BIOS
 */
void xhci_take_controller(struct xhci_hcd *controller) {
    uint32_t cparams;

    cparams = controller->cap_regs->hccparams1;

    size_t eoff = (((cparams & 0xFFFF0000) >> 16) * 4);
    volatile uint32_t *extcap = (size_t)controller->cap_regs + eoff;
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

        uint32_t *old = extcap;
        extcap = (size_t)extcap + (((val >> 8) & 0xFF) << 2);
        if (old == extcap) {
            break;
        }
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

    xhci_take_controller(controller);

    // TODO do this only on controller which require it
    //(at least intel panther and lynx point chipsets)
    xhci_switch_ports(pci_dev);

    // Set up irqs
    controller->irq_line = get_empty_int_vector();
    if (!pci_register_msi(pci_dev, controller->irq_line)) {
        io_apic_connect_gsi_to_vec(0, controller->irq_line, pci_dev->gsi,
                                   pci_dev->gsi_flags, 1);
    }

    int32_t cmd = controller->op_regs->usbcmd;
    cmd &= ~0x1;
    controller->op_regs->usbcmd = cmd | 1 << 1;
    // Wait for controller not ready
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

    // enable interrupts for this interrupter
    controller->run_regs->irs[0].iman |= 0x10;

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
