#ifndef __USB_H__
#define __USB_H__

#include <stddef.h>
#include <stdint.h>

#define USB_EP_TYPE_CONTROL 0
#define USB_EP_TYPE_ISOC    1
#define USB_EP_TYPE_BULK    2
#define USB_EP_TYPE_INTR    3

struct usb_hc_t;

struct usb_request_t {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed));

struct usb_device_descriptor_t {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_sub_class;
    uint8_t protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_release;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t serial_number;
    uint8_t num_configs;
} __attribute__((packed));

struct usb_config_t {
    uint8_t length;
    uint8_t type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t config_value;
    uint8_t config;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed));

struct usb_interface_t {
    uint8_t length;
    uint8_t type;
    uint8_t number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t class;
    uint8_t sub_class;
    uint8_t protocol;
    uint8_t interface;
} __attribute__((packed));

struct usb_endpoint_data_t {
    uint8_t length;
    uint8_t type;
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

struct usb_endpoint_t {
    uint8_t address;
    uint8_t attributes;
    int hcd_ep_num;
};

struct usb_dev_t {
    int address;
    int low_speed;
    int speed;
    size_t max_packet_size;
    struct usb_hc_t *controller;
    void *internal_controller;
    int num_endpoints;
    int claimed;
    int hcd_devno;
    struct usb_interface_t interface;

    uint16_t vendor;
    uint16_t product;
    uint16_t usb_ver;
    uint16_t dev_ver;
    uint8_t class;
    uint8_t subclass;

    int num_configs;
};

struct usb_hc_t {
    int (*send_control)(struct usb_dev_t *, void *, struct usb_request_t, int);
    int (*send_bulk)(struct usb_dev_t *, char *, size_t, int epno, int);
    int (*probe)(void *);
    int (*setup_endpoint)(struct usb_dev_t *, int, int);
};

#define MATCH_VENDOR      (1 << 0)
#define MATCH_PRODUCT     (1 << 1)
#define MATCH_USB_VERSION (1 << 2)
#define MATCH_DEV_VERSION (1 << 3)
#define MATCH_CLASS       (1 << 4)
#define MATCH_SUBCLASS    (1 << 5)

struct usb_driver_t {
    uint8_t match;
    uint16_t vendor;
    uint16_t product;
    uint16_t usb_ver;
    uint16_t dev_ver;
    uint8_t class;
    uint8_t subclass;

    int (*probe)(struct usb_dev_t *, struct usb_endpoint_t *);
};

int usb_make_request(struct usb_dev_t *, char *, size_t, uint8_t, uint8_t,
                     uint16_t, uint16_t, uint8_t, uint8_t);
int usb_add_device(struct usb_dev_t, int devno);
int usb_get_endpoint(struct usb_endpoint_t *endpoints, int ep_type, int in);
void usb_register_driver(struct usb_driver_t driver);
void init_usb(void);

#endif