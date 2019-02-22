#include <devices/backend/usb/uhci.h>
#include <devices/backend/usb/usb.h>
#include <misc/pci.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <lib/cio.h>
#include <lib/errno.h>

struct uhci_td_t {
    uint32_t link_pointer;
    uint32_t status;
    uint32_t token;
    uint32_t buffer_pointer;
    uint32_t reserved[4];
}__attribute__((packed));

struct uhci_queue_head_t {
    uint32_t link_pointer;
    uint32_t element_pointer;
}__attribute__((packed));

struct uhci_controller_t {
    uint32_t *frame_list;
    struct usb_dev_t devices[2];
    uint64_t io;
}__attribute__((aligned(4096)));

#define USBCMD 0x0
#define USBSTS 0x2
#define USBINTR 0x4
#define FRNUM 0x6
#define FRBASEADD 0x8
#define SOFMOD 0xC
#define PORTSC1 0x10
#define PORTSC2 0x12
#define USBLEGACY 0xC0

#define TD_STATUS_ACTIVE (1 << 23)
#define TD_TYPE_IN 0x69
#define TD_TYPE_OUT 0xE1
#define TD_TYPE_SETUP 0x2D

#define PORT_RESET (1 << 9)
#define PORT_ENABLED (1 << 2)
#define PORT_CONNECT (1)

struct uhci_controller_t uhci_controller;

static void uhci_set_w(uint64_t port, uint16_t value) {
    uint16_t data = port_in_w(port);
    data |= value;
    port_out_w(port, data);
}

static void uhci_clear_w(uint64_t port, uint16_t value) {
    uint16_t data = port_in_w(port);
    data &= ~value;
    port_out_w(port, data);
}

static int uhci_wait_for_queue(struct uhci_queue_head_t *queue) {
    struct uhci_td_t *td = (struct uhci_td_t *)(uintptr_t) ((queue->
        element_pointer & ~0xf) + MEM_PHYS_OFFSET);
    while (1) {
        if (queue->element_pointer & 1 || !(queue->element_pointer & ~0xf))
            return 0;
        td = (struct uhci_td_t *)(uintptr_t) ((queue->
            element_pointer & ~0xf) + MEM_PHYS_OFFSET);
        if (queue->element_pointer & 1 || !(queue->element_pointer & ~0xf))
            return 0;
        if (!td) return 0;
        if (!(td->status & TD_STATUS_ACTIVE)) break;
        relaxed_sleep(1);
    }
    if ((td->status & 0x7ff) != (td->token >> 21)) return -1;
    if ((td->status >> 16) & 0x7f) return -1;
    return 0;
}

static void uhci_init_td(struct uhci_td_t *td, uint8_t packet_id,
        uint8_t device_addr, uint8_t endpoint, uint8_t low_speed, uint8_t toggle,
        size_t length, char *data) {
    length = (length - 1) & 0x7ff;
    if (data)
        td->buffer_pointer = (uint32_t)(uintptr_t) (data - MEM_PHYS_OFFSET);
    td->token = (packet_id) | (device_addr << 8) | (endpoint << 15) | (toggle << 19) |
        (length << 21);
    td->status |= (low_speed << 26) | TD_STATUS_ACTIVE;
}

static void uhci_init_queue(struct uhci_queue_head_t *queue, struct uhci_td_t *td) {
    queue->element_pointer = (uint32_t)(uintptr_t) (td - MEM_PHYS_OFFSET);
}

static void uhci_insert_queue(struct uhci_queue_head_t *queue) {
    uhci_controller.frame_list[0] = (uint32_t)(uintptr_t) (queue - MEM_PHYS_OFFSET) | (1 << 1);
}

static int uhci_send_control(struct usb_dev_t *device, char *data,
        size_t size, int endpoint, int out) {
    if (!data || !device)
        return EINVAL;
    struct uhci_td_t *head = kalloc(sizeof(struct uhci_td_t));
    if (!head)
        return ENOMEM;
    struct uhci_td_t *prev = head;

    uhci_init_td(head, TD_TYPE_SETUP, device->address, endpoint,
            device->low_speed, 0, sizeof(struct usb_request_t), data);

    data += sizeof(struct usb_request_t);
    size -= sizeof(struct usb_request_t);

    struct uhci_td_t *td = NULL;
    int toggle = 1;
    uint8_t packet_id = out ? TD_TYPE_OUT : TD_TYPE_IN;
    while (size) {
        size_t packet_size = size;
        if (packet_size > device->max_packet_size)
            packet_size = device->max_packet_size;
        td = kalloc(sizeof(struct uhci_td_t));
        if (!td)
            return ENOMEM;
        uhci_init_td(td, packet_id, device->address, endpoint,
                device->low_speed, toggle, packet_size, data);
        td->link_pointer = 1;
        prev->link_pointer = (uint32_t)(uintptr_t) (td - MEM_PHYS_OFFSET) |
            (1 << 2);
        toggle ^= 1;
        prev = td;
        data += packet_size;
        size -= packet_size;
    }

    td = kalloc(sizeof(struct uhci_td_t));
    if (!td)
        return ENOMEM;
    packet_id = out ? TD_TYPE_IN : TD_TYPE_OUT; /* opposite of before */
    uhci_init_td(td, packet_id, device->address, endpoint, device->low_speed,
            1, 0, 0);
    td->link_pointer = 1;
    prev->link_pointer = (uint32_t)(uintptr_t) (td - MEM_PHYS_OFFSET) |
        (1 << 2);

    struct uhci_queue_head_t *queue = kalloc(sizeof(struct uhci_queue_head_t));
    if (!queue)
        return ENOMEM;
    uhci_init_queue(queue, head);
    queue->link_pointer = 1;
    uhci_insert_queue(queue);
    return uhci_wait_for_queue(queue);
}

static int uhci_send_bulk(struct usb_dev_t *device, char *data,
        size_t size, int endpoint_no, int out) {
    if (!data || !device)
        return EINVAL;

    struct usb_endpoint_t *endpoint = &device->endpoints[endpoint_no];
    unsigned int max_packet_size = endpoint->max_packet_size & 0x7ff;
    int toggle = 0;
    uint8_t packet_id = out ? TD_TYPE_OUT : TD_TYPE_IN;

    struct uhci_td_t *head = NULL;
    struct uhci_td_t *prev = NULL;
    struct uhci_td_t *td = NULL;
    while (size) {
        size_t packet_size = size;
        if (packet_size > max_packet_size)
            packet_size = max_packet_size;

        td = kalloc(sizeof(struct uhci_td_t));
        if (!td)
            return ENOMEM;
        if (!head)
            head = td;

        uhci_init_td(td, packet_id, device->address, endpoint_no + 1,
                device->low_speed, toggle, packet_size, data);
        td->link_pointer = 1;
        if (prev) {
            prev->link_pointer = (uint32_t)(uintptr_t) (td - MEM_PHYS_OFFSET) |
                (1 << 2);
        }
        toggle ^= 1;
        prev = td;
        data += packet_size;
        size -= packet_size;
    }

    struct uhci_queue_head_t *queue = kalloc(sizeof(struct uhci_queue_head_t));
    if (!queue)
        return ENOMEM;
    uhci_init_queue(queue, head);
    queue->link_pointer = 1;
    uhci_insert_queue(queue);
    return uhci_wait_for_queue(queue);
}

static int uhci_reset_port(int port) {
    int reg = uhci_controller.io + PORTSC1 + (port * 2);
    uhci_set_w(reg, PORT_RESET);
    relaxed_sleep(50);
    uhci_clear_w(reg, PORT_RESET);
    relaxed_sleep(10);
    uhci_set_w(reg, PORT_ENABLED);
    relaxed_sleep(100);
    uint16_t status = port_in_w(reg);
    if (!(status & PORT_CONNECT) || !(status & PORT_ENABLED))
        return -1;
    return 0;
}

static int uhci_probe(struct usb_hc_t *controller) {
    for (int i = 0; i < 2; i++) {
        if (!uhci_reset_port(i)) {
            kprint(KPRN_INFO, "usb/uhci: found device on port %u!", i);

            struct usb_dev_t device;
            uint16_t portsc = port_in_w(uhci_controller.io + PORTSC1 + (i * 2));
            device.low_speed = (portsc >> 8) & 1;
            device.max_packet_size = 8;
            device.controller = controller;
            if (usb_add_device(device)) return -1;
        }
    }
    return 0;
}

struct usb_hc_t *usb_init_uhci(void) {
    struct pci_device_t pci_dev = {0};
    int ret = pci_get_device(&pci_dev, 0x0C, 0x03);
    if (ret) {
        kprint(KPRN_INFO, "usb/uhci: could not find uhci controller!");
        return NULL;
    }
    kmemset(&uhci_controller, 0, sizeof(struct uhci_controller_t));
    uhci_controller.io = (pci_read_device(&pci_dev, 0x20) & ~0xf);
    if (!uhci_controller.io)
        return NULL;

    uhci_controller.frame_list = kalloc(1024 * sizeof(uint32_t));

    port_out_w(uhci_controller.io + USBLEGACY, 0x2000);
    /* turn interrupts off */
    port_out_w(uhci_controller.io + USBINTR, 0);
    /* setup frame list */
    port_out_w(uhci_controller.io + FRNUM, 0);
    port_out_d(uhci_controller.io + FRBASEADD, (uint32_t)(size_t)
            (uhci_controller.frame_list - MEM_PHYS_OFFSET));
    /* clear status */
    port_out_w(uhci_controller.io + USBSTS, 0xffff);
    /* enable controller */
    port_out_w(uhci_controller.io, 1);

    for (int i = 0; i < 1024; i++)
        uhci_controller.frame_list[i] = 1;

    struct usb_hc_t *controller = kalloc(sizeof(struct usb_hc_t));
    controller->send_control = uhci_send_control;
    controller->send_bulk = uhci_send_bulk;
    controller->probe = uhci_probe;

    kprint(KPRN_INFO, "usb/uhci: uhci controller initialised!");
    return controller;
}
