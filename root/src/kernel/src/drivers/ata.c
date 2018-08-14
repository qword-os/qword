#include <stdint.h>
#include <stddef.h>
#include <cio.h>
#include <klib.h>
#include <dev.h>
#include <ata.h>

#define DEVICE_COUNT 4
#define BYTES_PER_SECT 512

#define MAX_CACHED_SECTORS 8192

#define CACHE_NOT_READY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2

typedef struct {
    uint8_t *cache;
    uint64_t sector;
    int status;
} cached_sector_t;

typedef struct {
    int exists;

    int master;

    uint16_t identify[256];

    uint16_t data_port;
    uint16_t error_port;
    uint16_t sector_count_port;
    uint16_t lba_low_port;
    uint16_t lba_mid_port;
    uint16_t lba_hi_port;
    uint16_t device_port;
    uint16_t command_port;
    uint16_t control_port;

    uint64_t sector_count;
    uint16_t bytes_per_sector;

    cached_sector_t *cache;
} ata_device;

static const char *ata_names[] = {
    "hda", "hdb", "hdc", "hdd",
    "hde", "hdf", "hdg", "hdh",
    "hdi", "hdj", "hdk", "hdl",
    "hdm", "hdn", "hdo", "hdp",
    "hdq", "hdr", "hds", "hdt",
    "hdu", "hdv", "hdw", "hdx",
    "hdy", "hdz", NULL
};

static const uint16_t ata_ports[] = { 0x1f0, 0x1f0, 0x170, 0x170 };
static const int max_ports = 4;

static int ata_read(int drive, void *buf, uint64_t loc, size_t count);
static int ata_write(int drive, void *buf, uint64_t loc, size_t count);
static ata_device init_ata_device(uint16_t port_base, int master);
static void ata_identify(ata_device* dev);
static int ata_read28(int disk, uint32_t sector, uint8_t *buffer);
static int ata_read48(int disk, uint64_t sector, uint8_t *buffer);
static int ata_write28(int disk, uint32_t sector, uint8_t *buffer);
static int ata_write48(int disk, uint64_t sector, uint8_t *buffer);
static int ata_flush(int disk);
static int ata_flush_ext(int disk);

static ata_device devices[DEVICE_COUNT];

static int find_sect(int drive, uint64_t sect) {

    for (size_t i = 0; i < MAX_CACHED_SECTORS; i++)
        if ((devices[drive].cache[i].sector == sect)
            && (devices[drive].cache[i].status))
            return i;

    return -1;

}

static int overwritten_slot = 0;

static int cache_sect(int drive, int sect) {
    int targ;
    int ret;

    // find empty sect
    for (targ = 0; targ < MAX_CACHED_SECTORS; targ++)
        if (!devices[drive].cache[targ].status) goto fnd;

    // slot not found, overwrite another
    if (overwritten_slot == MAX_CACHED_SECTORS)
        overwritten_slot = 0;
    
    targ = overwritten_slot++;
    
    // flush cache
    if (devices[drive].cache[targ].status == CACHE_DIRTY) {
        if (sect <= 0x0fffffff)
            ret = ata_write28(drive, (uint32_t)devices[drive].cache[targ].sector, devices[drive].cache[targ].cache);
        else
            ret = ata_write48(drive, devices[drive].cache[targ].sector, devices[drive].cache[targ].cache);
        
        if (ret == -1) return -1;
    }
    
    goto notfnd;
    
fnd:
    // kalloc cache
    devices[drive].cache[targ].cache = kalloc(BYTES_PER_SECT);

notfnd:

    // load into cache
    if (sect <= 0x0fffffff)
        ret = ata_read28(drive, (uint32_t)sect, devices[drive].cache[targ].cache);
    else
        ret = ata_read48(drive, sect, devices[drive].cache[targ].cache);
    
    if (ret == -1) return -1;
    
    devices[drive].cache[targ].sector = sect;
    devices[drive].cache[targ].status = CACHE_READY;

    return targ;
}

static int ata_read(int drive, void *buf, uint64_t loc, size_t count) {
    uint64_t sect_count = count / BYTES_PER_SECT;
    if (count % BYTES_PER_SECT) sect_count++;

    uint64_t cur_sect = loc / BYTES_PER_SECT;
    uint16_t initial_offset = loc % BYTES_PER_SECT;
    uint16_t final_offset = (count % BYTES_PER_SECT) + initial_offset;

    if (final_offset >= BYTES_PER_SECT) {
        sect_count++;
        final_offset -= BYTES_PER_SECT;
    }

    for (uint64_t i = 0; ; i++) {
        // cache the sector
        int slot = find_sect(drive, cur_sect);
        if (slot == -1)
            slot = cache_sect(drive, cur_sect);
        if (slot == -1)
            return -1;

        if (i == sect_count - 1) {
            /* last sector */
            kmemcpy(buf, devices[drive].cache[slot].cache, final_offset);
            /* no need to do anything, just leave */
            break;
        }

        if (i == 0) {
            /* first sector */
            kmemcpy(buf, &devices[drive].cache[slot].cache[initial_offset], BYTES_PER_SECT - initial_offset);
            buf += BYTES_PER_SECT - initial_offset;
        } else {
            kmemcpy(buf, devices[drive].cache[slot].cache, BYTES_PER_SECT);
            buf += BYTES_PER_SECT;            
        }

        cur_sect++;
    }

    return 0;
}

static int ata_write(int drive, void *buf, uint64_t loc, size_t count) {
    uint64_t sect_count = count / BYTES_PER_SECT;
    if (count % BYTES_PER_SECT) sect_count++;

    uint64_t cur_sect = loc / BYTES_PER_SECT;
    uint16_t initial_offset = loc % BYTES_PER_SECT;
    uint16_t final_offset = (count % BYTES_PER_SECT) + initial_offset;

    if (final_offset >= BYTES_PER_SECT) {
        sect_count++;
        final_offset -= BYTES_PER_SECT;
    }

    for (uint64_t i = 0; ; i++) {
        // cache the sector
        int slot = find_sect(drive, cur_sect);
        if (slot == -1)
            slot = cache_sect(drive, cur_sect);
        if (slot == -1)
            return -1;

        if (i == sect_count - 1) {
            /* last sector */
            kmemcpy(devices[drive].cache[slot].cache, buf, final_offset);
            devices[drive].cache[slot].status = CACHE_DIRTY;
            /* no need to do anything, just leave */
            break;
        }

        if (i == 0) {
            /* first sector */
            kmemcpy(&devices[drive].cache[slot].cache[initial_offset], buf, BYTES_PER_SECT - initial_offset);
            devices[drive].cache[slot].status = CACHE_DIRTY;
            buf += BYTES_PER_SECT - initial_offset;
        } else {
            kmemcpy(devices[drive].cache[slot].cache, buf, BYTES_PER_SECT);
            devices[drive].cache[slot].status = CACHE_DIRTY;
            buf += BYTES_PER_SECT;            
        }

        cur_sect++;
    }

    return 0;
}

void init_ata(void) {
    kprint(KPRN_INFO, "ata: Initialising ata device driver...");
    
    int j = 0;
    int master = 1;
    for (int i = 0; i < DEVICE_COUNT; i++) {
        if (j >= max_ports) return;
        while (!(devices[i] = init_ata_device(ata_ports[j], master)).exists) {
            j++;
            if (j >= max_ports) return;
            if (j % 2) master = 0;
            else master = 1;
        }
        j++;
        if (j % 2) master = 0;
        else master = 1;
        device_add(ata_names[i], i, devices[i].sector_count * 512,
                          &ata_read, &ata_write);
        kprint(KPRN_INFO, "ata: Initialised %s", ata_names[i]);
    }

    return;
}

static ata_device init_ata_device(uint16_t port_base, int master) {
    ata_device dev;
    
    dev.data_port = port_base;
    dev.error_port = port_base + 0x01;
    dev.sector_count_port = port_base + 0x02;
    dev.lba_low_port = port_base + 0x03;
    dev.lba_mid_port = port_base + 0x04;
    dev.lba_hi_port = port_base + 0x05;
    dev.device_port = port_base + 0x06;
    dev.command_port = port_base + 0x07;
    dev.control_port = port_base + 0x206;
    dev.exists = 0;
    dev.master = master;
    
    dev.bytes_per_sector = 512;
    
    dev.cache = kalloc(MAX_CACHED_SECTORS * sizeof(cached_sector_t));
    
    ata_identify(&dev);
    
    return dev;
}

static void ata_identify(ata_device* dev) {

    if (dev->master)
        port_out_b(dev->device_port, 0xa0);
    else
        port_out_b(dev->device_port, 0xb0);
        
    port_out_b(dev->sector_count_port, 0);
    port_out_b(dev->lba_low_port, 0);
    port_out_b(dev->lba_mid_port, 0);
    port_out_b(dev->lba_hi_port, 0);
    
    port_out_b(dev->command_port, 0xEC); // identify command
    
    if (!port_in_b(dev->command_port)) {
        dev->exists = 0;
        kprint(KPRN_INFO, "ata: No device found!");
        return;
    } else {
        int timeout = 0;
        while (port_in_b(dev->command_port) & 0b10000000) {
            if (++timeout == 100000) {
                dev->exists = 0;
                kprint(KPRN_WARN, "ata: Drive detection timed out. Skipping drive!");
                return;
            }
        }
    }
    
    // check for non-standard ATAPI
    if (port_in_b(dev->lba_mid_port) || port_in_b(dev->lba_hi_port)) {
        dev->exists = 0;
        kprint(KPRN_INFO, "ata: Non-standard ATAPI, ignoring.");
        return;
    }
    
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = port_in_b(dev->command_port);
        if (status & 0b00000001) {
            dev->exists = 0;
            kprint(KPRN_ERR, "ata: Error occured!");
            return;
        }
        if (status & 0b00001000) goto success;
    }
    dev->exists = 0;
    kprint(KPRN_WARN, "ata: drive detection timed out. Skipping drive!");
    return;

success:
    kprint(KPRN_INFO, "ata: Storing IDENTIFY info...");
    for (int i = 0; i < 256; i++)
        dev->identify[i] = port_in_w(dev->data_port);

    kmemcpy(&dev->sector_count, &dev->identify[100], sizeof(uint64_t));
    kprint(KPRN_INFO, "ata: Sector count: %u", dev->sector_count);
    
    kprint(KPRN_INFO, "ata: Device successfully identified!");
    
    dev->exists = 1;
    
    return;
}

static int ata_read28(int disk, uint32_t sector, uint8_t *buffer) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0xE0 | ((sector & 0x0F000000) >> 24));
    else
        port_out_b(devices[disk].device_port, 0xF0 | ((sector & 0x0F000000) >> 24));
    
    port_out_b(devices[disk].sector_count_port, 1);
    port_out_b(devices[disk].lba_low_port, sector & 0x000000FF);
    port_out_b(devices[disk].lba_mid_port, (sector & 0x0000FF00) >> 8);
    port_out_b(devices[disk].lba_hi_port, (sector & 0x00FF0000) >> 16);
    
    port_out_b(devices[disk].command_port, 0x20); // read command
 
    uint8_t status = port_in_b(devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01)) 
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error reading sector %u on drive %u", sector, disk);
        return -1;
    }
    
    for (int i = 0; i < 256; i++) {
        uint16_t wdata = port_in_w(devices[disk].data_port);
        
        int c = i * 2;
        buffer[c] = wdata & 0xFF;
        buffer[c + 1] = (wdata >> 8) & 0xFF;
    }
    
    return 0;
}

static int ata_read48(int disk, uint64_t sector, uint8_t *buffer) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0x40);
    else
        port_out_b(devices[disk].device_port, 0x50);
    
    port_out_b(devices[disk].sector_count_port, 0);   // sector count high byte
    port_out_b(devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    port_out_b(devices[disk].sector_count_port, 1);   // sector count low byte
    port_out_b(devices[disk].lba_low_port, (uint8_t)(sector & 0x000000000000FF));
    port_out_b(devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000000000FF00) >> 8));
    port_out_b(devices[disk].lba_hi_port, (uint8_t)((sector & 0x00000000FF0000) >> 16));
    
    port_out_b(devices[disk].command_port, 0x24); // read command
 
    uint8_t status = port_in_b(devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01))
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error reading sector %U on drive %u", sector, disk);
        return -1;
    }
    
    for (int i = 0; i < 256; i++) {
        uint16_t wdata = port_in_w(devices[disk].data_port);
        
        int c = i * 2;
        buffer[c] = wdata & 0xFF;
        buffer[c + 1] = (wdata >> 8) & 0xFF;
    }
    
    return 0;
}

static int ata_write28(int disk, uint32_t sector, uint8_t *buffer) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0xE0 | ((sector & 0x0F000000) >> 24));
    else
        port_out_b(devices[disk].device_port, 0xF0 | ((sector & 0x0F000000) >> 24));
    
    port_out_b(devices[disk].sector_count_port, 1);
    port_out_b(devices[disk].lba_low_port, sector & 0x000000FF);
    port_out_b(devices[disk].lba_mid_port, (sector & 0x0000FF00) >> 8);
    port_out_b(devices[disk].lba_hi_port, (sector & 0x00FF0000) >> 16);
    
    port_out_b(devices[disk].command_port, 0x30); // write command
 
    uint8_t status = port_in_b(devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01)) 
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error writing sector %u on drive %u", sector, disk);
        return -1;
    }
        
    for (int i = 0; i < 256; i ++) {
        int c = i * 2;
        
        uint16_t wdata = (buffer[c + 1] << 8) | buffer[c];
        
        port_out_w(devices[disk].data_port, wdata);
    }
    
    ata_flush(disk);
    
    return 0;
}

static int ata_write48(int disk, uint64_t sector, uint8_t *buffer) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0x40);
    else
        port_out_b(devices[disk].device_port, 0x50);
    
    port_out_b(devices[disk].sector_count_port, 0);   // sector count high byte
    port_out_b(devices[disk].lba_low_port, (uint8_t)((sector & 0x000000FF000000) >> 24));
    port_out_b(devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000FF00000000) >> 32));
    port_out_b(devices[disk].lba_hi_port, (uint8_t)((sector & 0x00FF0000000000) >> 40));
    port_out_b(devices[disk].sector_count_port, 1);   // sector count low byte
    port_out_b(devices[disk].lba_low_port, (uint8_t)(sector & 0x000000000000FF));
    port_out_b(devices[disk].lba_mid_port, (uint8_t)((sector & 0x0000000000FF00) >> 8));
    port_out_b(devices[disk].lba_hi_port, (uint8_t)((sector & 0x00000000FF0000) >> 16));
    
    port_out_b(devices[disk].command_port, 0x34); // EXT write command
 
    uint8_t status = port_in_b(devices[disk].command_port);
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01)) 
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error writing sector %U on drive %u", sector, disk);
        return -1;
    }
        
    for (int i = 0; i < 256; i ++) {
        int c = i * 2;
        
        uint16_t wdata = (buffer[c + 1] << 8) | buffer[c];
        
        port_out_w(devices[disk].data_port, wdata);
    }
    
    ata_flush_ext(disk);
    
    return 0;
}

static int ata_flush(int disk) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0xE0);
    else
        port_out_b(devices[disk].device_port, 0xF0);
    
    port_out_b(devices[disk].command_port, 0xE7); // cache flush command
    
    uint8_t status = port_in_b(devices[disk].command_port);
    
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01)) 
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error occured while flushing cache.");
        return -1;
    }
    
    return 0;

}

static int ata_flush_ext(int disk) {

    if (devices[disk].master)
        port_out_b(devices[disk].device_port, 0x40);
    else
        port_out_b(devices[disk].device_port, 0x50);
    
    port_out_b(devices[disk].command_port, 0xEA); // cache flush EXT command
    
    uint8_t status = port_in_b(devices[disk].command_port);
    
    while (((status & 0x80) == 0x80)
        && ((status & 0x01) != 0x01)) 
        status = port_in_b(devices[disk].command_port);
    
    if (status & 0x01) {
        kprint(KPRN_ERR, "ata: Error occured while flushing cache.");
        return -1;
    }
    
    return 0;

}
