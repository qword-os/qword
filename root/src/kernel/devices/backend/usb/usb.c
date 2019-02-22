#include <devices/backend/usb/usb.h>
#include <devices/backend/usb/uhci.h>
#include <devices/backend/usb/mass_storage.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <lib/dynarray.h>

static int device_address = 0;
dynarray_new(struct usb_dev_t, usb_devices);

static int usb_set_addr(struct usb_dev_t *device, int address) {
    device->address = 0;
    if (usb_make_request(device, NULL, 0, 0, 5, address, 0, 0, 1))
        return -1;
    device->address = address;
    return 0;
}

static int usb_get_device_descriptor(struct usb_dev_t *device,
        struct usb_device_descriptor_t *descriptor) {
    char *buffer = kalloc(sizeof(struct usb_device_descriptor_t));
    if (usb_make_request(device, buffer, sizeof(struct usb_device_descriptor_t),
                0b10000000, 6, (1 << 8), 0, 0, 0)) {
        kfree(buffer);
        return -1;
    }

    kmemcpy(descriptor, buffer, sizeof(struct usb_device_descriptor_t));
    return 0;
}

static int usb_get_max_packet_size(struct usb_dev_t *device) {
    char *buffer = kalloc(8);
    if (usb_make_request(device, buffer, 8, 0b10000000, 6, (1 << 8), 0,
                0, 0)) {
        kfree(buffer);
        return -1;
    }
    int ret = buffer[7];
    kfree(buffer);
    return ret;
}

int usb_make_request(struct usb_dev_t *device, char *buffer, size_t size,
        uint8_t type, uint8_t request_no, uint16_t value, uint16_t index,
        uint8_t endpoint, uint8_t out) {
    char *temp_buffer = kalloc(sizeof(struct usb_request_t) + size);
    struct usb_request_t *request = (struct usb_request_t *) temp_buffer;
    request->request_type= type;
    request->request = request_no;
    request->value = value;
    request->index = index;
    request->length = size;
    if (device->controller->send_control(device, temp_buffer, sizeof(struct
                    usb_request_t) + size, endpoint, out)) {
        kfree(temp_buffer);
        return -1;
    }
    kmemcpy(buffer, temp_buffer + sizeof(struct usb_request_t), size);
    kfree(temp_buffer);
    return 0;
}

int usb_add_device(struct usb_dev_t device) {
    int ret = usb_set_addr(&device, ++device_address);
    if (ret) return -1;

    relaxed_sleep(10);

    ret = usb_get_max_packet_size(&device);
    if (ret < 0) return -1;
    device.max_packet_size = ret;

    struct usb_device_descriptor_t dev_descriptor;
    ret = usb_get_device_descriptor(&device, &dev_descriptor);
    if (ret) return -1;

    /* get config */
    struct usb_config_t config;
    ret = usb_make_request(&device, (char *) &config, sizeof(struct
                usb_config_t), 0b10000000, 6, (2 << 8), 0, 0, 0);
    if (ret) return -1;

    /* set config */
    ret = usb_make_request(&device, NULL, 0, 0, 9, config.config_value, 0, 0,
            1);
    if (ret) return -1;

    /* get entire config space */
    char *config_buf = kalloc(config.total_length);
    ret = usb_make_request(&device, config_buf, config.total_length,
            0b10000000, 6, (2 << 8), 0, 0, 0);
    if (ret) return -1;

    struct usb_interface_t *interface = (struct usb_interface_t *) (
            config_buf + config.length);

    /* set interface */
    ret = usb_make_request(&device, NULL, 0, 0b00000001, 11, 0, 0, 0, 1);
    if (ret) return -1;

    /* get all the endpoints */
    device.num_endpoints = interface->num_endpoints;
    for (int i = 0; i < interface->num_endpoints; i++) {
        if (i >= 15) {
            kprint(KPRN_ERR, "usb: device interface has over 15 endpoints!");
            return -1;
        }

        struct usb_endpoint_t *endpoint = (struct usb_endpoint_t *) (
                config_buf + config.length + interface->length + (i *
                    sizeof(struct usb_endpoint_t)));
        device.endpoints[i] = *endpoint;
    }
    device.class = interface->class;
    device.sub_class = interface->sub_class;

    kprint(KPRN_INFO, "usb: initiated device with class %x and subclass %x!",
            device.class, device.sub_class);
    dynarray_add(struct usb_dev_t, usb_devices, &device);
    return 0;
}

void init_usb(void) {
    struct usb_hc_t *controller = usb_init_uhci();
    if (!controller) {
        kprint(KPRN_ERR, "usb: could not find a controller!");
        return;
    }
    controller->probe(controller);

    if (!init_mass_storage(dynarray_getelem(struct usb_dev_t, usb_devices,
                    0))) {
        dynarray_remove(usb_devices, 0);
        return;
    }
}
