#include <stdint.h>
#include <fs.h>
#include <klib.h>

#define SECTOR_SIZE 2048
#define FILE_TYPE 0
#define DIR_TYPE 1
#define FILE_FLAG_HIDDEN (1 << 0)
#define FILE_FLAG_DIR (1 << 1)
#define FILE_FLAG_ASSOCIATED (1 << 2)
#define FILE_FLAG_XATTR_FORMAT (1 << 3)
#define FILE_FLAG_XATTR_PERMS (1 << 4)
#define FILE_FLAG_LARGE (1 << 7)
#define PATH_MAX 4096

struct int16_LSB_MSB_t {
    uint16_t little;
    uint16_t big;
}__attribute((packed));

struct int32_LSB_MSB_t {
    uint32_t little;
    uint32_t big;
}__attribute__((packed));

struct dir_date_time_t {
    uint8_t years;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t gmt_offset;
}__attribute((packed));

struct dec_datetime {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char hundreth_second[2];
    uint8_t gmt_offset;
}__attribute((packed));

struct directory_entry_t {
    uint8_t length;
    uint8_t xattr_length;
    struct int32_LSB_MSB_t extent_location;
    struct int32_LSB_MSB_t extent_length;
    struct dir_date_time_t date;
    uint8_t flags;
    uint8_t unit_size;
    uint8_t interleave_size;
    struct int16_LSB_MSB_t volume_sequence_number;
    uint8_t name_length;
    char name[];
}__attribute((packed));

struct descriptor_header_t {
    uint8_t type;
    char standard_identifier[5];
    uint8_t version;
}__attribute__((packed));

struct primary_descriptor_t {
    struct descriptor_header_t header;
    uint8_t unused1;
    char system_identifier[32];
    char volume_identifier[32];
    uint8_t unused2[8];
    struct int32_LSB_MSB_t volume_space_size;
    uint8_t unused3[32];
    struct int16_LSB_MSB_t volume_set_size;
    struct int16_LSB_MSB_t volume_sequence_number;
    struct int16_LSB_MSB_t logical_block_size;
    struct int32_LSB_MSB_t path_table_size;
    uint32_t l_path_table_location;
    uint32_t optional_l_path_table_location;
    uint32_t m_path_table_location;
    uint32_t optional_m_path_table_location;
    uint8_t length;
    uint8_t xattr_length;
    struct int32_LSB_MSB_t extent_location;
    struct int32_LSB_MSB_t extent_length;
    struct dir_date_time_t date;
    uint8_t flags;
    uint8_t unit_size;
    uint8_t interleave_size;
    struct int16_LSB_MSB_t root_entry_volume_sequence_number;
    uint8_t name_length;
    char name[1];
    char volume_set_name[128];
    char volume_publisher_name[128];
    char data_preparer_name[128];
    char application_name[128];
    char copyright_name[38];
    char abstract_name[36];
    char bibliographic_name[37];
    struct dec_datetime creation_date;
    struct dec_datetime modification_date;
    struct dec_datetime expiration_date;
    struct dec_datetime effective_date;
    uint8_t file_struct_version;
    uint8_t unused4[513];
    uint8_t reserved[653];
}__attribute__((packed));

struct path_result_t {
    struct directory_entry_t *target;
    struct directory_entry_t *parent;
    int failure;
    int not_found;
};

struct directory_result_t {
    char *entries;
    int num_entries;
    int failure;
};

struct mount_t {
    char name[128];
    int device;
    uint32_t num_blocks;
    uint16_t block_size;
    uint32_t path_table_size;
    uint32_t path_table_loc;
    struct directory_entry_t root_entry;
};

struct handle_t {
    int free;
    int mount;
    int flags;
    int mode;
    long offset;
    long begin;
    long end;
    struct path_result_t path_res;
    char path[PATH_MAX];
};

int handle_i = 0;
struct handle_t *handles;

int mount_i = 0;
struct mount_t *mounts;

static uint8_t rd_byte(int handle, uint64_t location) {
    uint8_t buf[1];
    lseek(handle, location, SEEK_SET);
    read(handle, buf, 1);
    return buf[0];
}

static int next_sector(int location) {
    return (location + 2047) & ~2047;
} 

static struct directory_result_t load_dir(struct mount_t *mount,
      struct directory_entry_t *dir) {
    struct directory_result_t result;
    if (!(dir->flags & FILE_FLAG_DIR)) {
      result.failure = 1;
      return result;
    }
    result.entries = kalloc(dir->extent_length.little);

    int count = 0;
    uint8_t length;
    for (uint32_t pos = 0; pos < dir->extent_length.little; pos += length) {
        length = rd_byte(mount->device,
                (dir->extent_location.little * SECTOR_SIZE) + pos);
        if (length == 0) {
            /* the end of the sector is padded */
            int offset = next_sector((dir->extent_location.little * SECTOR_SIZE)
                    + pos);
            pos += offset;
            continue;
        }

        lseek(mount->device, (dir->extent_location.little * SECTOR_SIZE) + pos,
                SEEK_SET);
        read(mount->device, result.entries + pos, length);
        count++;
    }
    result.num_entries = count;
    return result;
}

static int create_handle(struct handle_t handle) {
    for (int i = 0; i < handle_i; i++) {
        if (handles[i].free) {
            handles[i] = handle;
            return i;
        }
    }
    int handle_n = 0;
    handles = krealloc(handles, (handle_i + 1) * sizeof(struct handle_t));
    handle_n = handle_i++;
    handles[handle_n] = handle;
    return handle_n;
}

static struct path_result_t resolve_path(struct mount_t *mount,
        const char *path, int type) {
    struct path_result_t result = {0};
    
    if (*path == '/') path++;

    struct directory_entry_t *entry = NULL;
    struct directory_result_t current_dir = load_dir(mount, &mount->root_entry);
    do {
        const char *seg = path;
        path = kstrchrnul(path, '/');
        size_t seg_length = path - seg;

        if (seg_length == 1 && *seg == '.')
            seg = "\0";

        if (seg_length == 1 && seg[0] == '.' && seg[1] == '.') {
            seg = "\1";
            seg_length = 1;
        }

        if (result.target) {
            kfree(current_dir.entries);
            current_dir = load_dir(mount, result.target);
        }

        int found = 0;
        int pos = 0;
        for (int i = 0; i < current_dir.num_entries; i++) {
            entry = (struct directory_entry_t *)(current_dir.entries + pos);

            char lower_name[seg_length];
            for(size_t j = 0; j < seg_length; j++)
                lower_name[j] = ktolower(entry->name[j]);

            if (seg_length > entry->name_length)
                goto out;
            if (kstrncmp(seg, lower_name, seg_length) != 0)
                goto out;
            if (entry->name_length > seg_length && entry->name[seg_length]
                    != ';')
                goto out;
            found = 1;
            break;
out:
            pos += entry->length;
        }

        if (!found) {
            result.not_found = 1;
            return result;
        }

        result.parent = result.target;
        result.target = entry;
    } while(*path);


    if (!(result.target->flags & FILE_FLAG_DIR) && type == DIR_TYPE)
      result.failure = 1;
    return result;
}

static int iso9660_open(const char *path, int flags, int mode, int mount) {
    struct path_result_t result = resolve_path(&mounts[mount], path,
            FILE_TYPE);
    
    if (result.failure || result.not_found)
        return -1;

    struct handle_t handle = {0};
    kstrcpy(handle.path, path);
    handle.path_res = result;
    handle.flags = flags;
    handle.mode = mode;
    handle.mount = mount;
    handle.end = result.target->extent_length.little;
    if (flags & O_APPEND)
        handle.begin = handle.end;
    else
        handle.begin = result.target->extent_location.little;
    handle.offset = 0;
    return create_handle(handle);
}

static int iso9660_read(int handle, void *buf, size_t count) {
    struct handle_t *handle_s = &handles[handle];
    struct mount_t *mount = &mounts[handle_s->mount];

    if (((size_t)handle_s->offset + count) >= (size_t)handle_s->end)
        count = (size_t)(handle_s->offset - handle_s->end);
    if (!count)
        return -1;
    lseek(mount->device, (handle_s->begin * SECTOR_SIZE) +
            handle_s->offset, SEEK_SET);
    if (!buf)
        return -1;
    read(mount->device, buf, count);
    handle_s->offset += count;
    return (int)count;
}

static int iso9660_seek(int handle, off_t offset, int type) {
    if (handle < 0)
        return -1;
    if (handle > handle_i)
        return -1;
    if (handles[handle].free)
        return -1;
    struct handle_t *handle_s = &handles[handle];
    switch (type) {
        case SEEK_SET:
            handle_s->offset = offset;
            return handle_s->offset;
        case SEEK_CUR:
            handle_s->offset += offset;
            return handle_s->offset;
        case SEEK_END:
            handle_s->offset = handle_s->end;
            return handle_s->offset;
        default:
            return -1;
    }
}

/* TODO fix this, it's just a stub for size now */
static int iso9660_fstat(int handle, struct stat *st) {
    struct handle_t *handle_s = &handles[handle];
    st->st_size = handle_s->end;

    return 0;
}
static int iso9660_mount(const char *source) {
    int device = open(source, O_RDONLY, 0);

    uint8_t type = rd_byte(device, 0x10 * SECTOR_SIZE);
    if (type != 0x1) {
        kprint(KPRN_ERR, "iso9660: cannot find primary volume descriptor!");
        close(device);
        return -1;
    }

    struct primary_descriptor_t primary_descriptor;
    lseek(device, 0x10 * SECTOR_SIZE, SEEK_SET);
    read(device, &primary_descriptor, sizeof(struct primary_descriptor_t));

    mounts = krealloc(mounts, (mount_i + 1) * sizeof(struct mount_t));
    struct mount_t *mount = &mounts[mount_i];
    kstrcpy(mount->name, source);
    mount->device = device;
    mount->num_blocks = primary_descriptor.volume_space_size.little;
    mount->block_size = primary_descriptor.logical_block_size.little;
    mount->path_table_size = primary_descriptor.path_table_size.little;
    mount->path_table_loc = primary_descriptor.l_path_table_location;
    kmemcpy(&mount->root_entry, &primary_descriptor.length, 34);

    return mount_i++;
}

void init_iso9660(void) {
    struct fs_t iso9660 = {0};

    kstrcpy(iso9660.type, "iso9660");
    iso9660.mount = (void *)iso9660_mount;
    iso9660.open = iso9660_open;
    iso9660.read = iso9660_read;
    iso9660.lseek = iso9660_seek;
    iso9660.fstat = iso9660_fstat;

    vfs_install_fs(iso9660);
}
