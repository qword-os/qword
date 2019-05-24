#include <stdint.h>
#include <stddef.h>
#include <lib/cmdline.h>
#include <devices/display/vbe/vbe.h>
#include <fs/devfs/devfs.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <sys/panic.h>
#include <mm/mm.h>
#include "vbe_private.h"

int vbe_available = 0;

void get_vbe_info(struct vbe_info_struct_t *);
void get_edid_info(struct edid_info_struct_t *);
void get_vbe_mode_info(struct get_vbe_t *);
void set_vbe_mode(uint16_t);

static struct vbe_info_struct_t vbe_info_struct;
static struct edid_info_struct_t edid_info_struct;
static struct vbe_mode_info_t vbe_mode_info;
static struct get_vbe_t get_vbe;
static uint16_t vid_modes[1024];

uint32_t *vbe_framebuffer;
int vbe_width;
int vbe_height;
int vbe_pitch;

static void edid_call(void) {
    kprint(KPRN_INFO, "vbe: Calling EDID...");

    get_edid_info(&edid_info_struct);

    vbe_width = (int)edid_info_struct.det_timing_desc1[2];
    vbe_width += ((int)edid_info_struct.det_timing_desc1[4] & 0xf0) << 4;
    vbe_height = (int)edid_info_struct.det_timing_desc1[5];
    vbe_height += ((int)edid_info_struct.det_timing_desc1[7] & 0xf0) << 4;

    if (!vbe_width || !vbe_height) {
        kprint(KPRN_WARN, "vbe: EDID returned 0, defaulting to 1024x768");
        vbe_width = 1024;
        vbe_height = 768;
    }

    kprint(KPRN_INFO, "vbe: EDID resolution: %ux%u", (uint32_t)vbe_width, (uint32_t)vbe_height);

    return;
}

void init_vbe(void) {
    kprint(KPRN_INFO, "vbe: Initialising...");

    get_vbe_info(&vbe_info_struct);
    /* copy the video mode array somewhere else because it might get overwritten */
    for (size_t i = 0; ; i++) {
        vid_modes[i] = ((uint16_t *)((size_t)vbe_info_struct.vid_modes + MEM_PHYS_OFFSET))[i];
        if (((uint16_t *)(size_t)vbe_info_struct.vid_modes)[i+1] == 0xffff) {
            vid_modes[i+1] = 0xffff;
            break;
        }
    }

    kprint(KPRN_INFO, "vbe: Version: %u.%u", vbe_info_struct.version_maj, vbe_info_struct.version_min);
    kprint(KPRN_INFO, "vbe: OEM: %s", (char *)((size_t)vbe_info_struct.oem + MEM_PHYS_OFFSET));
    kprint(KPRN_INFO, "vbe: Graphics vendor: %s", (char *)((size_t)vbe_info_struct.vendor + MEM_PHYS_OFFSET));
    kprint(KPRN_INFO, "vbe: Product name: %s", (char *)((size_t)vbe_info_struct.prod_name + MEM_PHYS_OFFSET));
    kprint(KPRN_INFO, "vbe: Product revision: %s", (char *)((size_t)vbe_info_struct.prod_rev + MEM_PHYS_OFFSET));

    char *cmdline_val;
    if ((cmdline_val = cmdline_get_value("edid"))) {
        if (!strcmp(cmdline_val, "enabled")) {
            edid_call();
            goto modeset;
        }
    }

    if ((cmdline_val = cmdline_get_value("vbe_res"))) {
        if (!strcmp(cmdline_val, "1024x768")) {
            vbe_width = 1024;
            vbe_height = 768;
        } else if (!strcmp(cmdline_val, "800x600")) {
            vbe_width = 800;
            vbe_height = 600;
        } else if (!strcmp(cmdline_val, "640x480")) {
            vbe_width = 640;
            vbe_height = 480;
        } else {
            /* invalid vbe_res parameter panic */
            vbe_width = 1024;
            vbe_height = 768;
        }
    } else {
        /* with no vbe_res parameter, default to 1024x768 */
        vbe_width = 1024;
        vbe_height = 768;
    }

    kprint(KPRN_INFO, "vbe: Target resolution: %ux%u", (uint32_t)vbe_width, (uint32_t)vbe_height);

modeset:
    /* try to set the mode */
    get_vbe.vbe_mode_info = (uint32_t)((size_t)&vbe_mode_info - KERNEL_PHYS_OFFSET);
    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        get_vbe.mode = vid_modes[i];
        get_vbe_mode_info(&get_vbe);
        if (vbe_mode_info.res_x == vbe_width
            && vbe_mode_info.res_y == vbe_height
            && vbe_mode_info.bpp == 32) {
            /* mode found */
            kprint(KPRN_INFO, "vbe: Found matching mode %x, attempting to set.", (uint32_t)get_vbe.mode);
            vbe_framebuffer = (uint32_t *)((size_t)vbe_mode_info.framebuffer + MEM_PHYS_OFFSET);
            vbe_pitch = vbe_mode_info.pitch;
            kprint(KPRN_INFO, "vbe: Framebuffer address: %X", (size_t)vbe_mode_info.framebuffer + MEM_PHYS_OFFSET);
            set_vbe_mode(get_vbe.mode);
            /* Make the framebuffer write-combining */
            size_t fb_pages = ((vbe_pitch * vbe_height) + PAGE_SIZE - 1) / PAGE_SIZE;
            for (size_t i = 0; i < fb_pages; i++) {
                remap_page(kernel_pagemap, (size_t)vbe_framebuffer + i * PAGE_SIZE, 0x03 | (1 << 7) | (1 << 3));
            }
            goto success;
        }
    }

    /* modeset failed, panic */
    panic("VESA VBE modesetting failed.", 0, 0, NULL);

success:
    vbe_available = 1;
    kprint(KPRN_INFO, "vbe: Init done.");
    return;
}

static lock_t vesafb_lock = new_lock;

static int vesafb_write(int unused1, const void *buf, uint64_t loc, size_t count) {
    (void)unused1;

    spinlock_acquire(&vesafb_lock);
    for (size_t i = 0; i < count; i++)
        ((uint8_t *)vbe_framebuffer + loc)[i] = ((uint8_t *)buf)[i];
    spinlock_release(&vesafb_lock);

    return (int)count;
}

static int vesafb_read(int unused1, void *buf, uint64_t loc, size_t count) {
    (void)unused1;

    spinlock_acquire(&vesafb_lock);
    for (size_t i = 0; i < count; i++)
         ((uint8_t *)buf)[i] = ((uint8_t *)vbe_framebuffer + loc)[i];
    spinlock_release(&vesafb_lock);

    return (int)count;
}

void init_dev_vesafb(void) {
    struct device_t device = {0};

    device.calls = default_device_calls;

    strcpy(device.name, "vesafb");
    device.size = vbe_pitch * vbe_height;
    device.calls.read = vesafb_read;
    device.calls.write = vesafb_write;
    device_add(&device);
}
