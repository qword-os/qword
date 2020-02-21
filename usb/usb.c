#include <devices/usb/mass_storage.h>
#include <lib/alloc.h>
#include <lib/dynarray.h>
#include <lib/klib.h>
#include <usb/hcd/xhci.h>

static int num_drivers = 0;
dynarray_new(struct usb_driver_t, usb_drivers);

static int usb_get_descriptor(struct usb_dev_t *device, uint8_t desc_type, uint8_t desc_index, void *dest, uint16_t size) {
    struct usb_request_t req = {0};
    req.length = size;
    req.request_type = 0b10000000;
    req.request = 6;
    req.value = (desc_type << 8) | desc_index;
    device->controller->send_control(device, dest, req, 0);
    return 0;
}

void *get_configuration_space(struct usb_dev_t *device, uint8_t num) {
    struct usb_config_t config = {0};;
    usb_get_descriptor(device, 2, 0, &config, sizeof(struct usb_config_t));
    kprint(KPRN_INFO, "usb: Device descriptor is %X bytes", config.total_length);
    void* res = kalloc(config.total_length);
    usb_get_descriptor(device, 2, num, res, config.total_length);
    return res;
}

void usb_set_configuration(struct usb_dev_t *device, uint8_t num) {
    kprint(KPRN_INFO, "usb: setting configuration");
    struct usb_request_t req = {0};
    req.length = 0;
    req.request = 9;
    req.value = num;
    device->controller->send_control(device, NULL, req, 1);
}

void usb_set_interface(struct usb_dev_t *device, uint8_t int_num, uint8_t setting_num) {
    kprint(KPRN_INFO, "usb: setting interface");
    struct usb_request_t req = {0};
    req.length = 0;
    req.request_type = 0b00000001;
    req.request = 11;
    req.value = setting_num;
    req.value = int_num;
    device->controller->send_control(device, NULL, req, 1);
}

int usb_get_endpoint(struct usb_endpoint_t *endpoints, int ep_type, int in) {
    for(int i = 0; i < 15; i++) {
        int is_in = (endpoints[i].address & 0x80) > 0;
        int type = endpoints[i].attributes & 0x3;
        kprint(KPRN_INFO, "%X", type);
        if(is_in == in && ep_type == type) {
            return endpoints[i].hcd_ep_num;
        }
    }
    return -1;
}

int usb_add_device(struct usb_dev_t device, int devno) {
    kprint(KPRN_INFO, "usb: Adding device");
    struct usb_device_descriptor_t descriptor = {0};
    device.hcd_devno = devno;
    usb_get_descriptor(&device, 1, 0, &descriptor, sizeof(struct usb_device_descriptor_t));;
    kprint(KPRN_INFO, "Initialized device with vendor id: %x product id: %x class: %x subclass: %x",
           descriptor.vendor_id,
           descriptor.product_id,
           descriptor.device_class,
           descriptor.device_sub_class
    );
    device.num_configs = descriptor.num_configs;
    device.class = descriptor.device_class;
    device.subclass = descriptor.device_sub_class;
    device.vendor = descriptor.vendor_id;
    device.product = descriptor.product_id;
    device.usb_ver = descriptor.usb_version;
    device.dev_ver = descriptor.device_release;

    int assigned = 0;
    for (int i = 0; i < descriptor.num_configs; i++) {
        struct usb_config_t *config = get_configuration_space(&device, i);
        size_t interface_base = ((size_t)config + config->length);
        //TODO add support for matching vendor/product
        for (int j = 0; j < config->num_interfaces;) {
            struct usb_interface_t *interface = (struct usb_interface_t*)interface_base;
            interface_base += interface->length;
            if(interface->type != 0x04) {
                continue;
            }
            for (int k = 0; k < num_drivers; k++) {
                struct usb_driver_t *driver = dynarray_getelem(struct usb_driver_t, usb_drivers, k);
                if (!driver) {
                    break;
                }

                uint8_t match = driver->match;
                if(((match & MATCH_CLASS) && (driver->class == interface->class)) &&
                   ((match & MATCH_SUBCLASS) && (driver->subclass == interface->sub_class))) {
                    assigned = 1;

                    struct usb_endpoint_t endpoints[15] = {0};
                    size_t base = (size_t)interface + interface->length;
                    for(int l = 0; l < interface->num_endpoints;) {
                        struct usb_endpoint_data_t *ep_info = (struct usb_endpoint_data_t *) base;
                        base += ep_info->length;
                        if (ep_info->type != 0x05) {
                            continue;
                        } else {
                            endpoints[l].address = ep_info->address;
                            endpoints[l].attributes = ep_info->attributes;
                            endpoints[l].hcd_ep_num = device.controller->setup_endpoint(&device, ep_info->address, ep_info->max_packet_size);
                        }
                        l++;
                    }

                    usb_set_configuration(&device, config->config_value);
                    driver->probe(&device, endpoints);
                }
            }
            j++;
        }
        if(assigned) {
            break;
        }
    }
    return 0;
}

void usb_register_driver(struct usb_driver_t driver) {
    dynarray_add(struct usb_driver_t, usb_drivers, &driver);
    num_drivers++;
}

void init_usb(void) {
    struct usb_driver_t driver = {0};
    driver.match = MATCH_CLASS | MATCH_SUBCLASS;
    driver.probe = init_mass_storage;
    driver.class = 0x8;
    driver.subclass = 0x6;
    usb_register_driver(driver);
    usb_init_xhci();
}

