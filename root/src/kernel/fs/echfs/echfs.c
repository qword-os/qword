#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <fd/vfs/vfs.h>
#include <lib/lock.h>
#include <lib/errno.h>
#include <lib/ht.h>
#include <sys/panic.h>

#define SEARCH_FAILURE          0xffffffffffffffff
#define ROOT_ID                 0xffffffffffffffff
#define BYTES_PER_SECT          512
#define SECTORS_PER_BLOCK       (mnt->bytesperblock / BYTES_PER_SECT)
#define BYTES_PER_BLOCK         (mnt->bytesperblock)
#define ENTRIES_PER_SECT        2
#define ENTRIES_PER_BLOCK       (SECTORS_PER_BLOCK * ENTRIES_PER_SECT)
#define FILENAME_LEN            218
#define RESERVED_BLOCKS         16
#define FILE_TYPE               0
#define DIRECTORY_TYPE          1
#define DELETED_ENTRY           0xfffffffffffffffe
#define RESERVED_BLOCK          0xfffffffffffffff0
#define END_OF_CHAIN            0xffffffffffffffff

#define CACHE_NOTREADY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2

#define MAX_ECHFS_HANDLES 32768
#define MAX_ECHFS_MOUNTS 1024
#define MAX_CACHED_BLOCKS 8192
#define MAX_CACHED_FILES 8192

struct entry_t {
    uint64_t parent_id;
    uint8_t type;
    char name[FILENAME_LEN];
    uint8_t perms;
    uint16_t owner;
    uint16_t group;
    uint64_t time;
    uint64_t payload;
    uint64_t size;
}__attribute__((packed));

struct path_result_t {
    uint64_t target_entry;
    struct entry_t target;
    struct entry_t parent;
    char name[FILENAME_LEN];
    int failure;
    int not_found;
    uint8_t type;
};

struct cached_block_t {
    uint8_t *cache;
    uint64_t block;
    int status;
};

struct cached_file_t {
    char name[2048];
    struct path_result_t path_res;
    struct cached_block_t *cached_blocks;
    int overwritten_slot;
    uint64_t *alloc_map;
    uint64_t total_blocks;
    int changed_entry;
    int changed_cache;
};

struct mount_t {
    lock_t lock;
    int free;
    char name[128];
    int device;
    uint64_t blocks;
    uint64_t bytesperblock;
    uint64_t fatsize;
    uint64_t fatstart;
    uint64_t dirsize;
    uint64_t dirstart;
    uint64_t datastart;
    ht_new(struct cached_file_t, cached_files);
    int cached_files_ptr;
};

struct echfs_handle_t {
    int free;
    int refcount;
    uint8_t type;
    int mnt;
    char path[1024];
    int flags;
    uint64_t ptr;
    uint64_t end;
    struct cached_file_t *cached_file;
};

static struct echfs_handle_t **echfs_handles;
static lock_t echfs_handle_lock = 1;
static struct mount_t **mounts;
static lock_t echfs_mount_lock = 1;

static inline uint8_t rd_byte(int handle, uint64_t loc) {
    uint8_t buf[1];
    lseek(handle, loc, SEEK_SET);
    read(handle, buf, 1);
    return buf[0];
}

static inline void wr_byte(int handle, uint64_t loc, uint8_t val) {
    lseek(handle, loc, SEEK_SET);
    write(handle, (void *)&val, 1);
    return;
}

static inline uint16_t rd_word(int handle, uint64_t loc) {
    uint16_t buf[1];
    lseek(handle, loc, SEEK_SET);
    read(handle, buf, 2);
    return buf[0];
}

static inline void wr_word(int handle, uint64_t loc, uint16_t val) {
    lseek(handle, loc, SEEK_SET);
    write(handle, (void *)&val, 2);
    return;
}

static inline uint32_t rd_dword(int handle, uint64_t loc) {
    uint32_t buf[1];
    lseek(handle, loc, SEEK_SET);
    read(handle, buf, 4);
    return buf[0];
}

static inline void wr_dword(int handle, uint64_t loc, uint32_t val) {
    lseek(handle, loc, SEEK_SET);
    write(handle, (void *)&val, 4);
    return;
}

static inline uint64_t rd_qword(int handle, uint64_t loc) {
    uint64_t buf[1];
    lseek(handle, loc, SEEK_SET);
    read(handle, buf, 8);
    return buf[0];
}

static inline void wr_qword(int handle, uint64_t loc, uint64_t val) {
    lseek(handle, loc, SEEK_SET);
    write(handle, (void *)&val, 8);
    return;
}

static inline void rd_entry(struct entry_t *entry_src, struct mount_t *mnt, uint64_t entry) {
    uint64_t loc = (mnt->dirstart * mnt->bytesperblock) + (entry * sizeof(struct entry_t));
    lseek(mnt->device, loc, SEEK_SET);
    read(mnt->device, (void *)entry_src, sizeof(struct entry_t));

    return;
}

static inline void wr_entry(struct mount_t *mnt, uint64_t entry, struct entry_t *entry_src) {
    uint64_t loc = (mnt->dirstart * mnt->bytesperblock) + (entry * sizeof(struct entry_t));
    lseek(mnt->device, loc, SEEK_SET);
    write(mnt->device, (void *)entry_src, sizeof(struct entry_t));

    return;
}

static int synchronise_cached_file(struct mount_t *mnt, struct cached_file_t *cached_file) {
    if (cached_file->changed_entry) {
        wr_entry(mnt, cached_file->path_res.target_entry, &cached_file->path_res.target);
        cached_file->changed_entry = 0;
    }
    if (cached_file->path_res.type != FILE_TYPE)
        return 0;
    if (cached_file->changed_cache) {
        struct cached_block_t *cached_blocks = cached_file->cached_blocks;
        for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
            if (cached_blocks[i].status == CACHE_DIRTY) {
                lseek(mnt->device,
                      cached_file->alloc_map[cached_blocks[i].block] * BYTES_PER_BLOCK,
                      SEEK_SET);
                write(mnt->device,
                      cached_blocks[i].cache,
                      BYTES_PER_BLOCK);
                cached_blocks[i].status = CACHE_READY;
            }
        }
        cached_file->changed_cache = 0;
    }
    return 0;
}

static int echfs_sync(void) {
    spinlock_acquire(&echfs_mount_lock);
    for (size_t i = 0; i < MAX_ECHFS_MOUNTS; i++) {
        if (!mounts[i])
            break;
        if (mounts[i]->free)
            continue;
        struct mount_t *mnt = mounts[i];

        spinlock_acquire(&mnt->cached_files_lock);
        size_t total_cached_files;
        struct cached_file_t **cached_files = ht_dump(struct cached_file_t, mnt->cached_files, &total_cached_files);

        for (size_t i = 0; i < total_cached_files; i++) {
            spinlock_acquire(&mnt->lock);
            synchronise_cached_file(mnt, cached_files[i]);
            spinlock_release(&mnt->lock);
        }

        kfree(cached_files);
        spinlock_release(&mnt->cached_files_lock);
    }
    spinlock_release(&echfs_mount_lock);
    return 0;
}

static uint64_t allocate_empty_block(struct mount_t *mnt, uint64_t prev_block) {
    lseek(mnt->device, mnt->fatstart * mnt->bytesperblock, SEEK_SET);
    uint64_t i;
    for (i = 0; ; i++) {
        uint64_t block;
        read(mnt->device, &block, sizeof(uint64_t));
        if (block == 0) {
            block = END_OF_CHAIN;
            lseek(mnt->device, -(sizeof(uint64_t)), SEEK_CUR);
            write(mnt->device, &block, sizeof(uint64_t));
            if (prev_block) {
                /* Go to prev block and target it to this */
                lseek(mnt->device, mnt->fatstart * mnt->bytesperblock
                    + prev_block * sizeof(uint64_t), SEEK_SET);
                write(mnt->device, &i, sizeof(uint64_t));
            }
            break;
        }
    }
    return i;
}

// free on disk allocated space for this file and flush its cache
static int erase_file(struct mount_t *mnt, struct cached_file_t *cached_file) {
    // erase block chain first
    uint64_t empty = 0;
    uint64_t block = cached_file->path_res.target.payload;
    if (block != END_OF_CHAIN) {
        for (;;) {
            lseek(mnt->device, mnt->fatstart * mnt->bytesperblock
                    + block * sizeof(uint64_t), SEEK_SET);
            read(mnt->device, &block, sizeof(uint64_t));
            lseek(mnt->device, -(sizeof(uint64_t)), SEEK_CUR);
            write(mnt->device, &empty, sizeof(uint64_t));
            if (block == END_OF_CHAIN)
                break;
        }
    }
    // clean up cache
    struct cached_block_t *cached_blocks = cached_file->cached_blocks;
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (cached_blocks[i].status) {
            cached_blocks[i].status = CACHE_NOTREADY;
            kfree(cached_blocks[i].cache);
        }
    }
    // clean up metadata
    cached_file->path_res.target.payload = END_OF_CHAIN;
    cached_file->path_res.target.size = 0;
    cached_file->total_blocks = 0;
    kfree(cached_file->alloc_map);
    cached_file->alloc_map = kalloc(sizeof(uint64_t));
    cached_file->changed_entry = 1;
    cached_file->changed_cache = 0;

    return 0;
}

static int find_block(int handle, uint64_t block) {
    struct cached_block_t *cached_blocks =
        echfs_handles[handle]->cached_file->cached_blocks;

    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((cached_blocks[i].block == block)
            && (cached_blocks[i].status))
            return i;

    return -1;
}

static int cache_block(int handle, uint64_t block) {
    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];
    struct cached_file_t *cached_file = echfs_handles[handle]->cached_file;
    struct cached_block_t *cached_blocks =
        echfs_handles[handle]->cached_file->cached_blocks;
    int targ;

    /* Find empty block */
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!cached_blocks[targ].status) goto fnd;

    /* Slot not find, overwrite another */
    if (cached_file->overwritten_slot == MAX_CACHED_BLOCKS)
        cached_file->overwritten_slot = 0;

    targ = cached_file->overwritten_slot++;

    /* Flush device cache */
    if (cached_blocks[targ].status == CACHE_DIRTY) {
        lseek(mnt->device,
              cached_file->alloc_map[cached_blocks[targ].block] * BYTES_PER_BLOCK,
              SEEK_SET);
        write(mnt->device,
              cached_blocks[targ].cache,
              BYTES_PER_BLOCK);
    }

    goto notfnd;

fnd:
    /* Allocate some cache for this device */
    cached_blocks[targ].cache = kalloc(BYTES_PER_BLOCK);

notfnd:

    /* Check if said block exists */
    if (block >= cached_file->total_blocks) {
        uint64_t new_block_count = block + 1;
        cached_file->alloc_map = krealloc(cached_file->alloc_map,
                    new_block_count * sizeof(uint64_t));
        for (uint64_t i = cached_file->total_blocks; i < new_block_count; i++) {
            uint64_t new_block;
            if (!i) {
                new_block = allocate_empty_block(mnt, 0);
                cached_file->path_res.target.payload = new_block;
                cached_file->changed_entry = 1;
            } else {
                new_block = allocate_empty_block(mnt, cached_file->alloc_map[i - 1]);
            }
            cached_file->alloc_map[i] = new_block;
        }
        cached_file->total_blocks = new_block_count;
    }

    /* Load sector into cache */
    lseek(mnt->device,
          cached_file->alloc_map[block] * BYTES_PER_BLOCK,
          SEEK_SET);
    read(mnt->device,
         cached_blocks[targ].cache,
         BYTES_PER_BLOCK);

    cached_blocks[targ].block = block;
    cached_blocks[targ].status = CACHE_READY;

    return targ;
}

static int echfs_read(int handle, void *buf, size_t count) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    if (echfs_handles[handle]->type == DIRECTORY_TYPE) {
        spinlock_release(&mnt->lock);
        errno = EISDIR;
        return -1;
    }

    if ((size_t)echfs_handles[handle]->ptr + count
      >= (size_t)echfs_handles[handle]->end) {
        count -= ((size_t)echfs_handles[handle]->ptr + count) - (size_t)echfs_handles[handle]->end;
    }

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the block */
        uint64_t block = (echfs_handles[handle]->ptr + progress) / BYTES_PER_BLOCK;
        int slot = find_block(handle, block);
        if (slot == -1) {
            slot = cache_block(handle, block);
            if (slot == -1) {
                spinlock_release(&mnt->lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (echfs_handles[handle]->ptr + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

        kmemcpy(buf + progress, &echfs_handles[handle]->cached_file->cached_blocks[slot].cache[offset], chunk);
        progress += chunk;
    }

    echfs_handles[handle]->ptr += count;

    spinlock_release(&mnt->lock);
    return (int)count;
}

static int echfs_write(int handle, const void *buf, size_t count) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    if (echfs_handles[handle]->type == DIRECTORY_TYPE) {
        spinlock_release(&mnt->lock);
        errno = EISDIR;
        return -1;
    }

    if (echfs_handles[handle]->flags & O_APPEND)
        echfs_handles[handle]->ptr = echfs_handles[handle]->end;

    uint64_t progress = 0;
    while (progress < count) {
        /* cache the block */
        uint64_t block = (echfs_handles[handle]->ptr + progress) / BYTES_PER_BLOCK;
        int slot = find_block(handle, block);
        if (slot == -1) {
            slot = cache_block(handle, block);
            if (slot == -1) {
                spinlock_release(&mnt->lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (echfs_handles[handle]->ptr + progress) % BYTES_PER_BLOCK;
        if (chunk > BYTES_PER_BLOCK - offset)
            chunk = BYTES_PER_BLOCK - offset;

        kmemcpy(&echfs_handles[handle]->cached_file->cached_blocks[slot].cache[offset], buf + progress, chunk);
        echfs_handles[handle]->cached_file->cached_blocks[slot].status = CACHE_DIRTY;
        echfs_handles[handle]->cached_file->changed_cache = 1;
        progress += chunk;
    }

    echfs_handles[handle]->ptr += count;

    if (echfs_handles[handle]->ptr > echfs_handles[handle]->end) {
        echfs_handles[handle]->end = echfs_handles[handle]->ptr;
        echfs_handles[handle]->cached_file->path_res.target.size = echfs_handles[handle]->ptr;
        echfs_handles[handle]->cached_file->changed_entry = 1;
    }

    spinlock_release(&mnt->lock);
    return (int)count;
}

static int echfs_create_handle(struct echfs_handle_t *handle) {
    spinlock_acquire(&echfs_handle_lock);

    int handle_n;

    /* Check for a free handle */
    for (size_t i = 0; i < MAX_ECHFS_HANDLES; i++) {
        if (!echfs_handles[i] || echfs_handles[i]->free) {
            handle_n = i;
            goto load_handle;
        }
    }

    /* TODO catch this and do something about it */
    panic("", 0, 0);

load_handle:
    echfs_handles[handle_n] = kalloc(sizeof(struct echfs_handle_t));
    *echfs_handles[handle_n] = *handle;

    spinlock_release(&echfs_handle_lock);
    return handle_n;
}

static int echfs_create_mount(struct mount_t *mount) {
    spinlock_acquire(&echfs_mount_lock);

    int mount_n;

    /* Check for a free handle */
    for (size_t i = 0; i < MAX_ECHFS_MOUNTS; i++) {
        if (!mounts[i] || mounts[i]->free) {
            mount_n = i;
            goto load_mount;
        }
    }

    /* TODO catch this and do something about it */
    panic("", 0, 0);

load_mount:
    mounts[mount_n] = kalloc(sizeof(struct mount_t));
    *mounts[mount_n] = *mount;

    spinlock_release(&echfs_mount_lock);
    return mount_n;
}

static uint64_t search(struct mount_t *mnt, const char *name, uint64_t parent, uint8_t *type) {
    struct entry_t entry;
    // returns unique entry #, SEARCH_FAILURE upon failure/not found
    uint64_t loc = mnt->dirstart * mnt->bytesperblock;
    lseek(mnt->device, loc, SEEK_SET);
    for (uint64_t i = 0; ; i++) {
        if (i >= (mnt->dirsize * ENTRIES_PER_BLOCK)) return SEARCH_FAILURE;  // check if past directory table
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) return SEARCH_FAILURE;              // check if past last entry
        if ((entry.parent_id == parent) && (!kstrcmp(entry.name, name))) {
            *type = entry.type;
            return i;
        }
    }
}

static uint64_t find_free_entry(struct mount_t *mnt) {
    uint64_t i;
    struct entry_t entry;

    uint64_t loc = mnt->dirstart * mnt->bytesperblock;
    lseek(mnt->device, loc, SEEK_SET);

    for (i = 0; ; i++) {
        if (i >= (mnt->dirsize * ENTRIES_PER_BLOCK)) return SEARCH_FAILURE;  // check if past directory table
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) break;              // check if past last entry
        if (entry.parent_id == DELETED_ENTRY) break;
    }

    return i;
}

static uint64_t get_free_id(struct mount_t *mnt) {
    uint64_t id = 1;
    uint64_t i;
    struct entry_t entry;

    uint64_t loc = mnt->dirstart * mnt->bytesperblock;
    lseek(mnt->device, loc, SEEK_SET);

    for (i = 0; ; i++) {
        if (i >= (mnt->dirsize * ENTRIES_PER_BLOCK)) return SEARCH_FAILURE;  // check if past directory table
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) break;              // check if past last entry
        if (entry.parent_id == DELETED_ENTRY) continue;
        if ((entry.type == 1) && (entry.payload == id))
            id = (entry.payload + 1);
    }

    return id;
}

static void path_resolver(struct path_result_t *path_result, struct mount_t *mnt, const char *path) {
    // returns a struct of useful info
    // failure flag set upon failure
    // not_found flag set upon not found
    // even if the file is not found, info about the "parent"
    // directory and name are still returned
    char name[FILENAME_LEN];
    int last = 0;
    int i;
    struct entry_t empty_entry = {0};

    path_result->name[0] = 0;
    path_result->target_entry = 0;
    path_result->parent = empty_entry;
    path_result->target = empty_entry;
    path_result->failure = 0;
    path_result->not_found = 0;
    path_result->type = DIRECTORY_TYPE;

    path_result->parent.payload = ROOT_ID;

    if (!kstrcmp(path, "/")) {
        kstrcpy(path_result->name, "/");
        path_result->target_entry = -1;
        path_result->target.parent_id = -1;
        path_result->target.type = DIRECTORY_TYPE;
        kstrcpy(path_result->target.name, "/");
        path_result->target.payload = ROOT_ID;
        return; // exception for root
    }

    if (*path == '/') path++;

next:
    for (i = 0; *path != '/'; path++) {
        if (!*path) {
            last = 1;
            break;
        }
        name[i++] = *path;
    }
    name[i] = 0;
    path++;

    if (!last) {
        uint8_t type;
        uint64_t search_res = search(mnt, name, path_result->parent.payload, &type);
        if (search_res == SEARCH_FAILURE || type != DIRECTORY_TYPE) {
            path_result->failure = 1; // fail if search fails
            return;
        }
        rd_entry(&path_result->parent, mnt, search_res);
    } else {
        uint64_t search_res = search(mnt, name, path_result->parent.payload, &path_result->type);
        if (search_res == SEARCH_FAILURE)
            path_result->not_found = 1;
        else {
            rd_entry(&path_result->target, mnt, search_res);
            path_result->target_entry = search_res;
        }
        kstrcpy(path_result->name, name);
        return;
    }

    goto next;
}

static int echfs_close(int handle) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    if (!(--echfs_handles[handle]->refcount)) {
        echfs_handles[handle]->free = 1;
    }

    spinlock_release(&mnt->lock);
    return 0;
}

static struct cached_file_t *cache_file(struct mount_t *mnt, const char *path) {
    struct cached_file_t *ret = ht_get(struct cached_file_t, mnt->cached_files, path);
    if (ret)
        return ret;

    struct path_result_t path_result;
    path_resolver(&path_result, mnt, path);
    if (path_result.failure)
        return NULL;

    struct cached_file_t *cached_file = kalloc(sizeof(struct cached_file_t));
    if (!cached_file)
        return NULL;

    kstrcpy(cached_file->name, path);
    cached_file->path_res = path_result;

    if (path_result.not_found || path_result.type == FILE_TYPE) {
        cached_file->cached_blocks =
            kalloc(MAX_CACHED_BLOCKS * sizeof(struct cached_block_t));

        cached_file->total_blocks = 0;
        cached_file->alloc_map = kalloc(sizeof(uint64_t));
    }

    if (!path_result.not_found && path_result.type == FILE_TYPE) {
        cached_file->alloc_map[0] = path_result.target.payload;

        uint64_t i;
        for (i = 1; cached_file->alloc_map[i-1] != END_OF_CHAIN; i++) {
            cached_file->alloc_map = krealloc(cached_file->alloc_map,
                                                sizeof(uint64_t) * (i + 1));
            cached_file->alloc_map[i] = rd_qword(mnt->device,
                    (mnt->fatstart * mnt->bytesperblock)
                  + (cached_file->alloc_map[i-1] * sizeof(uint64_t)));
        }

        cached_file->total_blocks = i - 1;
    }

    ht_add(struct cached_file_t, mnt->cached_files, cached_file);
    return cached_file;
}

static int echfs_open(const char *path, int flags, int mnt) {
    spinlock_acquire(&mounts[mnt]->lock);

    struct echfs_handle_t new_handle = {0};
    struct cached_file_t *cached_file = cache_file(mounts[mnt], path);
    if (!cached_file) {
        spinlock_release(&mounts[mnt]->lock);
        errno = ENOENT;
        return -1;
    }

    struct path_result_t *path_result = &cached_file->path_res;

    if (path_result->not_found && !(flags & O_CREAT)) {
        spinlock_release(&mounts[mnt]->lock);
        errno = ENOENT;
        return -1;
    }

    if (!path_result->not_found && flags & O_TRUNC) {
        erase_file(mounts[mnt], cached_file);
    }

    if (path_result->not_found && flags & O_CREAT) {
        // create new entry
        struct entry_t entry;

        entry.parent_id = path_result->parent.payload;
        entry.type = FILE_TYPE;
        kstrcpy(entry.name, path_result->name);
        entry.perms = 0; // TODO
        entry.owner = 0; // TODO
        entry.group = 0; // TODO
        entry.time = 0; // TODO
        entry.payload = END_OF_CHAIN;
        entry.size = 0;

        uint64_t new_entry = find_free_entry(mounts[mnt]);

        wr_entry(mounts[mnt], new_entry, &entry);

        path_result->target = entry;
        path_result->target_entry = new_entry;
        path_result->not_found = 0;
        path_result->type = FILE_TYPE;
    }

    new_handle.type = path_result->type;

    if (path_result->type == DIRECTORY_TYPE) {
        // it's a directory
        if ((flags & O_ACCMODE) == O_WRONLY
         || (flags & O_ACCMODE) == O_RDWR) {
            spinlock_release(&mounts[mnt]->lock);
            errno = EISDIR;
            return -1;
        }
    }

    kstrcpy(new_handle.path, path);
    new_handle.flags = flags;

    if (path_result->type == FILE_TYPE) {
        new_handle.end = path_result->target.size;
        new_handle.ptr = 0;
    }

    new_handle.mnt = mnt;

    new_handle.cached_file = cached_file;

    new_handle.free = 0;
    new_handle.refcount = 1;

    int ret = echfs_create_handle(&new_handle);
    spinlock_release(&mounts[mnt]->lock);
    return ret;
}

static int echfs_lseek(int handle, off_t offset, int type) {
    long ret;

    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    int flags = echfs_handles[handle]->flags;
    switch (type) {
        case SEEK_SET:
            if ((uint64_t)offset >= echfs_handles[handle]->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handles[handle]->ptr = offset;
            break;
        case SEEK_END:
            if (echfs_handles[handle]->end + offset >= echfs_handles[handle]->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handles[handle]->ptr = echfs_handles[handle]->end + offset;
            break;
        case SEEK_CUR:
            if (echfs_handles[handle]->ptr + offset >= echfs_handles[handle]->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handles[handle]->ptr += offset;
            break;
        default:
        einval:
            spinlock_release(&mnt->lock);
            errno = EINVAL;
            return -1;
    }

    ret = echfs_handles[handle]->ptr;
    spinlock_release(&mnt->lock);
    return ret;
}

static int echfs_dup(int handle) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    echfs_handles[handle]->refcount++;

    spinlock_release(&mnt->lock);

    return 0;
}

static int echfs_readdir(int handle, struct dirent *dir) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    uint64_t dir_id = echfs_handles[handle]->cached_file->path_res.target.payload;

    struct entry_t entry;
    uint64_t loc = mnt->dirstart * mnt->bytesperblock +
                    echfs_handles[handle]->ptr * sizeof(struct entry_t);
    lseek(mnt->device, loc, SEEK_SET);

    for (;;) {
        // check if past directory table
        if (echfs_handles[handle]->ptr >= (mnt->dirsize * ENTRIES_PER_BLOCK)) goto end_of_dir;
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) goto end_of_dir;              // check if past last entry
        echfs_handles[handle]->ptr++;
        if (entry.parent_id == dir_id) {
            // valid entry
            dir->d_ino = echfs_handles[handle]->ptr;
            kstrcpy(dir->d_name, entry.name);
            dir->d_reclen = sizeof(struct dirent);
            switch (entry.type) {
                case DIRECTORY_TYPE:
                    dir->d_type = DT_DIR;
                    break;
                case FILE_TYPE:
                    dir->d_type = DT_REG;
                    break;
            }
            break;
        }
    }

    spinlock_release(&mnt->lock);
    return 0;

end_of_dir:
    spinlock_release(&mnt->lock);
    errno = 0;
    return -1;
}

static int echfs_fstat(int handle, struct stat *st) {
    if (   handle < 0
        || handle >= MAX_ECHFS_HANDLES
        || echfs_handles[handle]->free) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = mounts[echfs_handles[handle]->mnt];

    spinlock_acquire(&mnt->lock);

    struct path_result_t *path_res = &echfs_handles[handle]->cached_file->path_res;

    st->st_dev = mnt->device;
    st->st_ino = path_res->target_entry + 1;
    st->st_nlink = 1;
    st->st_uid = path_res->target.owner;
    st->st_gid = path_res->target.group;
    st->st_rdev = 0;
    st->st_size = path_res->target.size;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 512 - 1) / 512;
    st->st_atim.tv_sec = path_res->target.time;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = path_res->target.time;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = path_res->target.time;
    st->st_ctim.tv_nsec = 0;

    st->st_mode = 0;
    switch (echfs_handles[handle]->type) {
        case DIRECTORY_TYPE:
            st->st_mode |= S_IFDIR;
            break;
        case FILE_TYPE:
            st->st_mode |= S_IFREG;
            break;
    }

    spinlock_release(&mnt->lock);
    return 0;
}

static int echfs_mount(const char *source) {

    /* open device */
    int device = open(source, O_RDWR);
    if (device == -1) {
        /* errno propagates from open */
        return -1;
    }

    /* verify signature */
    char signature[8];
    lseek(device, 4, SEEK_SET);
    read(device, signature, 8);
    if (kstrncmp(signature, "_ECH_FS_", 8)) {
        kprint(KPRN_ERR, "echidnaFS signature invalid, mount failed!");
        close(device);
        errno = EINVAL;
        return -1;
    }

    struct mount_t mount;

    mount.device = device;
    kstrcpy(mount.name, source);
    mount.blocks = rd_qword(device, 12);
    mount.bytesperblock = rd_qword(device, 28);
    mount.fatsize = (mount.blocks * sizeof(uint64_t)) / mount.bytesperblock;
    if ((mount.blocks * sizeof(uint64_t)) % mount.bytesperblock) mount.fatsize++;
    mount.fatstart = RESERVED_BLOCKS;
    mount.dirsize = rd_qword(device, 20);
    mount.dirstart = mount.fatstart + mount.fatsize;
    mount.datastart = RESERVED_BLOCKS + mount.fatsize + mount.dirsize;

    ht_init(mount.cached_files);

    mount.free = 0;
    mount.lock = 1;

    int ret = echfs_create_mount(&mount);

    return ret;
}

void init_fs_echfs(void) {
    echfs_handles = kalloc(MAX_ECHFS_HANDLES * sizeof(void *));
    if (!echfs_handles)
        panic("out of memory while allocating echfs_handles", 0, 0);

    mounts = kalloc(MAX_ECHFS_MOUNTS * sizeof(void *));
    if (!mounts)
        panic("out of memory while allocating echfs_mounts", 0, 0);

    struct fs_t echfs = {0};

    kstrcpy(echfs.name, "echfs");
    echfs.mount = (void *)echfs_mount;
    echfs.open = echfs_open;
    echfs.close = echfs_close;
    echfs.read = echfs_read;
    echfs.write = echfs_write;
    echfs.lseek = echfs_lseek;
    echfs.fstat = echfs_fstat;
    echfs.dup = echfs_dup;
    echfs.readdir = echfs_readdir;
    echfs.sync = echfs_sync;

    vfs_install_fs(&echfs);
}
