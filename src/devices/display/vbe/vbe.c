#include <stdint.h>
#include <stddef.h>
#include <devices/display/vbe/vbe.h>
#include <fs/devfs/devfs.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <mm/mm.h>
#include <lib/cstring.h>
#include <startup/stivale.h>

int vbe_available = 0;

uint32_t *vbe_framebuffer;
int vbe_width;
int vbe_height;
int vbe_pitch;

void init_vbe(struct stivale_framebuffer_t *fb) {
    kprint(KPRN_INFO, "vbe: Initialising...");

    vbe_framebuffer = (uint32_t *)(fb->address + MEM_PHYS_OFFSET);
    vbe_width       = (int)fb->width;
    vbe_height      = (int)fb->height;
    vbe_pitch       = (int)fb->pitch;
    vbe_available   = 1;

    kprint(KPRN_INFO, "vbe: Init done.");
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
