#include <stdint.h>
#include <stddef.h>
#include <vbe.h>
#include <klib.h>
#include <cmdline.h>

static vbe_info_struct_t vbe_info_struct;
static edid_info_struct_t edid_info_struct;
static vbe_mode_info_t vbe_mode_info;
static get_vbe_t get_vbe;
static uint16_t vid_modes[1024];

uint32_t *vbe_framebuffer;
int vbe_width;
int vbe_height;

int vbe_available = 0;

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
    /* interrupts are supposed to be OFF */
    char *cmdline_val;

    if ((cmdline_val = cmdline_get_value("display"))) {
        if (!kstrcmp(cmdline_val, "vga")) {
            vbe_available = 0;
            return;
        } else if (!kstrcmp(cmdline_val, "vbe")) {
            ;
        } else {
            for (;;);
        }
    }

    kprint(KPRN_INFO, "vbe: Initialising...");

    get_vbe_info(&vbe_info_struct);
    /* copy the video mode array somewhere else because it might get overwritten */
    for (size_t i = 0; ; i++) {
        vid_modes[i] = ((uint16_t *)(size_t)vbe_info_struct.vid_modes)[i];
        if (((uint16_t *)(size_t)vbe_info_struct.vid_modes)[i+1] == 0xffff) {
            vid_modes[i+1] = 0xffff;
            break;
        }
    }

    kprint(KPRN_INFO, "vbe: Version: %u.%u", vbe_info_struct.version_maj, vbe_info_struct.version_min);
    kprint(KPRN_INFO, "vbe: OEM: %s", (char *)(size_t)vbe_info_struct.oem);
    kprint(KPRN_INFO, "vbe: Graphics vendor: %s", (char *)(size_t)vbe_info_struct.vendor);
    kprint(KPRN_INFO, "vbe: Product name: %s", (char *)(size_t)vbe_info_struct.prod_name);
    kprint(KPRN_INFO, "vbe: Product revision: %s", (char *)(size_t)vbe_info_struct.prod_rev);

    if ((cmdline_val = cmdline_get_value("edid"))) {
        if (!kstrcmp(cmdline_val, "enabled")) {
            edid_call();
            goto modeset;
        } else if (!kstrcmp(cmdline_val, "disabled")) {
            ;
        } else {
            /* invalid edid parameter panic */
            for (;;);
        }
    }

    if ((cmdline_val = cmdline_get_value("vbe_res"))) {
        if (!kstrcmp(cmdline_val, "1024x768")) {
            vbe_width = 1024;
            vbe_height = 768;
        } else if (!kstrcmp(cmdline_val, "800x600")) {
            vbe_width = 800;
            vbe_height = 600;
        } else if (!kstrcmp(cmdline_val, "640x480")) {
            vbe_width = 640;
            vbe_height = 480;
        } else {
            /* invalid vbe_res parameter panic */
            for (;;);
        }
    } else {
        /* with no vbe_res parameter, default to 1024x768 */
        vbe_width = 1024;
        vbe_height = 768;
    }

    kprint(KPRN_INFO, "vbe: Target resolution: %ux%u", (uint32_t)vbe_width, (uint32_t)vbe_height);

modeset:
    /* try to set the mode */
    get_vbe.vbe_mode_info = (uint32_t)(size_t)&vbe_mode_info;
    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        get_vbe.mode = vid_modes[i];
        get_vbe_mode_info(&get_vbe);
        if (vbe_mode_info.res_x == vbe_width
            && vbe_mode_info.res_y == vbe_height
            && vbe_mode_info.bpp == 32) {
            /* mode found */
            kprint(KPRN_INFO, "vbe: Found matching mode %x, attempting to set.", (uint32_t)get_vbe.mode);
            vbe_framebuffer = (uint32_t *)(size_t)vbe_mode_info.framebuffer;
            kprint(KPRN_INFO, "vbe: Framebuffer address: %x", (uint32_t)vbe_mode_info.framebuffer);
            set_vbe_mode(get_vbe.mode);
            goto success;
        }
    }

    /* modeset failed panic */
    for (;;);

success:
    vbe_available = 1;
    kprint(KPRN_INFO, "vbe: Init done.");
    return;
}
