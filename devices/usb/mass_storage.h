#ifndef __USB_MASS_STORAGE_H__
#define __USB_MASS_STORAGE_H__

#include <usb/usb.h>

int init_mass_storage(struct usb_dev_t *device, struct usb_endpoint_t *endpoints);

#endif
