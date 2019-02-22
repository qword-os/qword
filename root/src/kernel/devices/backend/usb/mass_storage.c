#include <devices/backend/usb/mass_storage.h>
#include <devices/frontend/scsi.h>
#include <lib/dynarray.h>
#include <lib/klib.h>
#include <lib/bit.h>
#include <fs/devfs/devfs.h>

#define MAX_CACHED_BLOCKS 8192
#define CBW_SIGNATURE 0x43425355
#define CBW_DEV_TO_HOST (1 << 7)

/* command block wrapper */
struct cbw_t {
    uint32_t signature;
    uint32_t tag;
    uint32_t length;
    uint8_t direction;
    uint8_t lun;
    uint8_t command_length;
    uint8_t command[16];
}__attribute__((packed));

/* command status wrapper */
struct csw_t {
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
}__attribute__((packed));

struct mass_storage_dev_t {
    int in, out; /* endpoint numbers for in and out endpoints */
    struct usb_dev_t device;
};

dynarray_new(struct mass_storage_dev_t, devices);

static int mass_storage_send_cmd(int drive, char *cmd, size_t cmd_length,
        char *buf, size_t request_length, int out) {

    struct cbw_t cbw = {0};
    cbw.command_length = cmd_length;
    kmemcpy(&cbw.command, cmd, cmd_length);
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = 1;
    cbw.length = request_length;
    cbw.direction = out ? 0 : CBW_DEV_TO_HOST;

    struct mass_storage_dev_t *device = dynarray_getelem(struct
            mass_storage_dev_t, devices, drive);
    if (!device) {
        errno = EINVAL;
        return -1;
    }

    int ret = device->device.controller->send_bulk(&device->device, (char *) &cbw,
            sizeof(struct cbw_t), device->out, 1);
    if (ret) {
        errno = EIO;
        return -1;
    }

    int endpoint = out ? device->out : device->in;
    ret = device->device.controller->send_bulk(&device->device, buf,
            request_length, endpoint, out);
    if (ret) {
        errno = EIO;
        return -1;
    }

    struct csw_t csw = {0};
    ret = device->device.controller->send_bulk(&device->device,
            (char *) &csw, sizeof(struct csw_t), device->in, 0);
    if (ret || csw.status) {
        errno = EIO;
        return -1;
    }

    return 0;
}

int init_mass_storage(struct usb_dev_t *device) {
    if (device->class != 0x8 || device->sub_class != 0x6)
        return -1;

    struct mass_storage_dev_t internal_dev = {0};
    internal_dev.device = *device;

    /* determine endpoints */
    if (device->num_endpoints != 2)
        return -1;
    for (int i = 0; i < 2; i++) {
        if (device->endpoints[i].address >> 7) {
            internal_dev.in = i;
            continue;
        }
        internal_dev.out = i;
    }

    /* before we do anything, reset the device */
    int ret = usb_make_request(device, NULL, 0, 0b00100001, 0xFF, 0, 0, 0, 1);
    if (ret) return -1;

    ret = dynarray_add(struct mass_storage_dev_t, devices, &internal_dev);
    if (ret < 0) return -1;

    scsi_register(ret, mass_storage_send_cmd);

    kprint(KPRN_INFO, "mass_storage: initiated device!");

    return 0;
}
