#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <fd/vfs/vfs.h>
#include <lib/lock.h>
#include <lib/errno.h>
#include <lib/ht.h>
#include <sys/panic.h>
#include <lib/cstring.h>
#include <lib/cmem.h>

#define SEARCH_FAILURE          0xffffffffffffffff
#define ROOT_ID                 0xffffffffffffffff
#define BYTES_PER_SECT          512
#define ENTRIES_PER_SECT        2
#define FILENAME_LEN            201
#define RESERVED_BLOCKS         16
#define FILE_TYPE               0
#define DIRECTORY_TYPE          1
#define DELETED_ENTRY           0xfffffffffffffffe
#define RESERVED_BLOCK          0xfffffffffffffff0
#define END_OF_CHAIN            0xffffffffffffffff

#define CACHE_NOTREADY 0
#define CACHE_READY 1

#define MAX_CACHED_BLOCKS 8192

struct entry_t {
    uint64_t parent_id;
    uint8_t type;
    char name[FILENAME_LEN];
    uint64_t atime;
    uint64_t mtime;
    uint16_t perms;
    uint16_t owner;
    uint16_t group;
    uint64_t ctime;
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
    size_t refcount;
    int unlinked;
    struct mount_t *mnt;
    struct path_result_t path_res;
    struct cached_block_t *cached_blocks;
    int overwritten_slot;
    uint64_t *alloc_map;
    uint64_t total_blocks;
};

struct mount_t {
    lock_t lock;
    char name[128];
    int device;
    uint64_t blocks;
    uint64_t bytesperblock;
    uint64_t sectorsperblock;
    uint64_t entriesperblock;
    uint64_t fatsize;
    uint64_t fatstart;
    uint64_t dirsize;
    uint64_t dirstart;
    uint64_t datastart;
    ht_new(struct cached_file_t, cached_files);
    int cached_files_ptr;
};

struct echfs_handle_t {
    int refcount;
    uint8_t type;
    struct mount_t *mnt;
    char path[1024];
    int flags;
    uint64_t ptr;
    uint64_t end;
    struct cached_file_t *cached_file;
};

dynarray_new(struct echfs_handle_t, handles);
dynarray_new(struct mount_t, mounts);

static inline uint8_t rd_byte(int handle, uint64_t loc) {
    uint8_t buf[1];
    lseek(handle, loc, SEEK_SET);
    read(handle, buf, 1);
    return buf[0];
}

static inline void wr_byte(int handle, uint64_t loc, uint8_t val) {
    lseek(handle, loc, SEEK_SET);
    write(handle, (void *)&val, 1);
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
}

static inline void rd_entry(struct entry_t *entry_src, struct mount_t *mnt, uint64_t entry) {
    uint64_t loc = (mnt->dirstart * mnt->bytesperblock) + (entry * sizeof(struct entry_t));
    lseek(mnt->device, loc, SEEK_SET);
    read(mnt->device, (void *)entry_src, sizeof(struct entry_t));
}

static inline void wr_entry(struct mount_t *mnt, uint64_t entry, struct entry_t *entry_src) {
    uint64_t loc = (mnt->dirstart * mnt->bytesperblock) + (entry * sizeof(struct entry_t));
    lseek(mnt->device, loc, SEEK_SET);
    write(mnt->device, (void *)entry_src, sizeof(struct entry_t));
}

static void echfs_sync(void) {
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
static int erase_file(struct cached_file_t *cached_file) {
    struct mount_t *mnt = cached_file->mnt;
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
    wr_entry(mnt, cached_file->path_res.target_entry, &cached_file->path_res.target);

    return 0;
}

static int find_block(struct cached_file_t *cached_file, uint64_t block) {
    struct cached_block_t *cached_blocks = cached_file->cached_blocks;

    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((cached_blocks[i].block == block)
            && (cached_blocks[i].status))
            return i;

    return -1;
}

static int cache_block(struct cached_file_t *cached_file, uint64_t block) {
    struct cached_block_t *cached_blocks = cached_file->cached_blocks;
    struct mount_t *mnt = cached_file->mnt;
    int targ;

    /* Find empty block */
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!cached_blocks[targ].status) goto fnd;

    /* Slot not find, overwrite another */
    if (cached_file->overwritten_slot == MAX_CACHED_BLOCKS)
        cached_file->overwritten_slot = 0;

    targ = cached_file->overwritten_slot++;

    goto notfnd;

fnd:
    /* Allocate some cache for this device */
    cached_blocks[targ].cache = kalloc(mnt->bytesperblock);
    if (!cached_blocks[targ].cache)
        return -1;

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
                wr_entry(mnt, cached_file->path_res.target_entry, &cached_file->path_res.target);
            } else {
                new_block = allocate_empty_block(mnt, cached_file->alloc_map[i - 1]);
            }
            cached_file->alloc_map[i] = new_block;
        }
        cached_file->total_blocks = new_block_count;
    }

    /* Load sector into cache */
    lseek(mnt->device,
          cached_file->alloc_map[block] * mnt->bytesperblock,
          SEEK_SET);
    read(mnt->device,
         cached_blocks[targ].cache,
         mnt->bytesperblock);

    cached_blocks[targ].block = block;
    cached_blocks[targ].status = CACHE_READY;

    return targ;
}

static int echfs_read(int handle, void *buf, size_t count) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (echfs_handle->type == DIRECTORY_TYPE) {
        dynarray_unref(handles, handle);
        errno = EISDIR;
        return -1;
    }

    if ((size_t)echfs_handle->ptr + count >= (size_t)echfs_handle->end)
        count = (size_t)echfs_handle->end - (size_t)echfs_handle->ptr;

    struct mount_t *mnt = echfs_handle->mnt;

    spinlock_acquire(&mnt->lock);

    struct cached_file_t *cached_file = echfs_handle->cached_file;
    uint64_t progress = 0;
    while (progress < count) {
        /* cache the block */
        uint64_t block = (echfs_handle->ptr + progress) / mnt->bytesperblock;
        int slot = find_block(cached_file, block);
        if (slot == -1) {
            slot = cache_block(cached_file, block);
            if (slot == -1) {
                spinlock_release(&mnt->lock);
                dynarray_unref(handles, handle);
                errno = EIO;
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (echfs_handle->ptr + progress) % mnt->bytesperblock;
        if (chunk > mnt->bytesperblock - offset)
            chunk = mnt->bytesperblock - offset;

        memcpy(buf + progress, &cached_file->cached_blocks[slot].cache[offset], chunk);
        progress += chunk;
    }

    echfs_handle->ptr += count;

    spinlock_release(&mnt->lock);
    dynarray_unref(handles, handle);
    return (int)count;
}

static int echfs_write(int handle, const void *buf, size_t count) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (echfs_handle->type == DIRECTORY_TYPE) {
        dynarray_unref(handles, handle);
        errno = EISDIR;
        return -1;
    }

    struct mount_t *mnt = echfs_handle->mnt;

    spinlock_acquire(&mnt->lock);

    if (echfs_handle->flags & O_APPEND)
        echfs_handle->ptr = echfs_handle->end;

    struct cached_file_t *cached_file = echfs_handle->cached_file;
    uint64_t progress = 0;
    while (progress < count) {
        /* cache the block */
        uint64_t block = (echfs_handle->ptr + progress) / mnt->bytesperblock;
        int slot = find_block(cached_file, block);
        if (slot == -1) {
            slot = cache_block(cached_file, block);
            if (slot == -1) {
                spinlock_release(&mnt->lock);
                dynarray_unref(handles, handle);
                errno = EIO;
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (echfs_handle->ptr + progress) % mnt->bytesperblock;
        if (chunk > mnt->bytesperblock - offset)
            chunk = mnt->bytesperblock - offset;

        memcpy(&cached_file->cached_blocks[slot].cache[offset], buf + progress, chunk);

        struct cached_block_t *cached_blocks = cached_file->cached_blocks;
        lseek(mnt->device,
              cached_file->alloc_map[cached_blocks[slot].block] * mnt->bytesperblock,
              SEEK_SET);
        write(mnt->device,
              cached_blocks[slot].cache,
              mnt->bytesperblock);

        progress += chunk;
    }

    echfs_handle->ptr += count;

    if (echfs_handle->ptr > echfs_handle->end) {
        echfs_handle->end = echfs_handle->ptr;
        cached_file->path_res.target.size = echfs_handle->ptr;
        wr_entry(mnt, cached_file->path_res.target_entry, &cached_file->path_res.target);
    }

    spinlock_release(&mnt->lock);
    dynarray_unref(handles, handle);
    return (int)count;
}

static int actually_delete_file(struct cached_file_t *cached_file) {
    erase_file(cached_file);

    kfree(cached_file->alloc_map);
    kfree(cached_file->cached_blocks);
    kfree(cached_file);

    return 0;
}

static int echfs_unlink(int handle) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (echfs_handle->type == DIRECTORY_TYPE) {
        dynarray_unref(handles, handle);
        errno = EISDIR;
        return -1;
    }

    struct mount_t *mnt = echfs_handle->mnt;
    spinlock_acquire(&mnt->lock);

    struct cached_file_t *cached_file = echfs_handle->cached_file;

    cached_file->unlinked = 1;

    struct entry_t deleted_entry = {0};
    deleted_entry.parent_id = DELETED_ENTRY;
    wr_entry(mnt, cached_file->path_res.target_entry, &deleted_entry);

    ht_remove(struct cached_file_t, mnt->cached_files, cached_file->name);

    int ret = 0;
    if (!--cached_file->refcount)
        ret = actually_delete_file(cached_file);

    spinlock_release(&mnt->lock);
    return ret;
}

static uint64_t search(struct mount_t *mnt, const char *name, uint64_t parent, uint8_t *type) {
    struct entry_t entry;
    // returns unique entry #, SEARCH_FAILURE upon failure/not found
    uint64_t loc = mnt->dirstart * mnt->bytesperblock;
    lseek(mnt->device, loc, SEEK_SET);
    for (uint64_t i = 0; ; i++) {
        if (i >= (mnt->dirsize * mnt->entriesperblock)) return SEARCH_FAILURE;  // check if past directory table
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) return SEARCH_FAILURE;              // check if past last entry
        if ((entry.parent_id == parent) && (!strcmp(entry.name, name))) {
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
        if (i >= (mnt->dirsize * mnt->entriesperblock)) return SEARCH_FAILURE;  // check if past directory table
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
        if (i >= (mnt->dirsize * mnt->entriesperblock)) return SEARCH_FAILURE;  // check if past directory table
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

    if (!strcmp(path, "/")) {
        strcpy(path_result->name, "/");
        path_result->target_entry = -1;
        path_result->target.parent_id = -1;
        path_result->target.type = DIRECTORY_TYPE;
        strcpy(path_result->target.name, "/");
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
        if (search_res == SEARCH_FAILURE) {
            path_result->not_found = 1;
        } else {
            rd_entry(&path_result->target, mnt, search_res);
            path_result->target_entry = search_res;
        }
        strcpy(path_result->name, name);
        return;
    }

    goto next;
}

static int echfs_close(int handle) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    int ret = 0;

    struct mount_t *mnt = echfs_handle->mnt;
    spinlock_acquire(&mnt->lock);

    struct cached_file_t *cached_file = echfs_handle->cached_file;

    if (!--cached_file->refcount && cached_file->unlinked)
        ret = actually_delete_file(echfs_handle->cached_file);

    if (ret)
        goto out;

    if (!(--echfs_handle->refcount))
        dynarray_remove(handles, handle);

out:
    dynarray_unref(handles, handle);
    spinlock_release(&mnt->lock);
    return ret;
}

static struct cached_file_t *cache_file(struct mount_t *mnt, const char *path) {
    struct cached_file_t *ret = ht_get(struct cached_file_t, mnt->cached_files, path);
    if (ret) {
        ret->refcount++;
        return ret;
    }

    struct path_result_t path_result;
    path_resolver(&path_result, mnt, path);
    if (path_result.failure)
        return NULL;

    struct cached_file_t *cached_file = kalloc(sizeof(struct cached_file_t));
    if (!cached_file)
        return NULL;

    strcpy(cached_file->name, path);
    cached_file->path_res = path_result;

    cached_file->mnt = mnt;

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

    cached_file->unlinked = 0;
    cached_file->refcount = 1;

    ht_add(struct cached_file_t, mnt->cached_files, cached_file);
    return cached_file;
}

static int echfs_mkdir(const char *path, int m) {
    struct mount_t *mnt = dynarray_getelem(struct mount_t, mounts, m);
    if (!mnt)
        return -1;

    spinlock_acquire(&mnt->lock);

    struct cached_file_t *cached_file = cache_file(mnt, path);
    if (!cached_file) {
        spinlock_release(&mnt->lock);
        dynarray_unref(mounts, m);
        errno = ENOENT;
        return -1;
    }

    struct path_result_t *path_result = &cached_file->path_res;

    if (path_result->failure) {
        spinlock_release(&mnt->lock);
        dynarray_unref(mounts, m);
        errno = ENOTDIR;
        return -1;
    }

    if (!path_result->not_found) {
        spinlock_release(&mnt->lock);
        dynarray_unref(mounts, m);
        errno = EEXIST;
        return -1;
    }

    uint64_t new_entry = find_free_entry(mnt);
    uint64_t new_dir_id = get_free_id(mnt);

    // create new entry
    struct entry_t entry;

    entry.parent_id = path_result->parent.payload;
    entry.type = DIRECTORY_TYPE;
    strcpy(entry.name, path_result->name);
    entry.perms = 0; // TODO
    entry.owner = 0; // TODO
    entry.group = 0; // TODO
    entry.ctime = 0; // TODO
    entry.atime = 0; // TODO
    entry.mtime = 0; // TODO
    entry.payload = new_dir_id;
    entry.size = 0;

    wr_entry(mnt, new_entry, &entry);

    path_result->target = entry;
    path_result->target_entry = new_entry;
    path_result->not_found = 0;
    path_result->type = DIRECTORY_TYPE;

    spinlock_release(&mnt->lock);
    dynarray_unref(mounts, m);
    return 0;
}

static int echfs_open(const char *path, int flags, int m) {
    struct mount_t *mnt = dynarray_getelem(struct mount_t, mounts, m);
    if (!mnt)
        return -1;

    spinlock_acquire(&mnt->lock);

    struct echfs_handle_t new_handle = {0};
    struct cached_file_t *cached_file = cache_file(mnt, path);
    if (!cached_file) {
        spinlock_release(&mnt->lock);
        dynarray_unref(mounts, m);
        errno = ENOENT;
        return -1;
    }

    struct path_result_t *path_result = &cached_file->path_res;

    if (path_result->not_found && !(flags & O_CREAT)) {
        spinlock_release(&mnt->lock);
        dynarray_unref(mounts, m);
        errno = ENOENT;
        return -1;
    }

    if (!path_result->not_found && flags & O_TRUNC)
        erase_file(cached_file);

    if (path_result->not_found && flags & O_CREAT) {
        // create new entry
        struct entry_t entry;

        entry.parent_id = path_result->parent.payload;
        entry.type = FILE_TYPE;
        strcpy(entry.name, path_result->name);
        entry.perms = 0; // TODO
        entry.owner = 0; // TODO
        entry.group = 0; // TODO
        entry.ctime = 0; // TODO
        entry.atime = 0; // TODO
        entry.mtime = 0; // TODO
        entry.payload = END_OF_CHAIN;
        entry.size = 0;

        uint64_t new_entry = find_free_entry(mnt);

        wr_entry(mnt, new_entry, &entry);

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
            spinlock_release(&mnt->lock);
            dynarray_unref(mounts, m);
            errno = EISDIR;
            return -1;
        }
    }

    strcpy(new_handle.path, path);
    new_handle.flags = flags;

    if (path_result->type == FILE_TYPE) {
        new_handle.end = path_result->target.size;
        new_handle.ptr = 0;
    }

    new_handle.mnt = mnt;

    new_handle.cached_file = cached_file;

    new_handle.refcount = 1;

    int ret = dynarray_add(struct echfs_handle_t, handles, &new_handle);
    spinlock_release(&mnt->lock);
    dynarray_unref(mounts, m);
    return ret;
}

static int echfs_lseek(int handle, off_t offset, int type) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = echfs_handle->mnt;

    spinlock_acquire(&mnt->lock);

    int flags = echfs_handle->flags;
    switch (type) {
        case SEEK_SET:
            if ((uint64_t)offset >= echfs_handle->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handle->ptr = offset;
            break;
        case SEEK_END:
            if (echfs_handle->end + offset >= echfs_handle->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handle->ptr = echfs_handle->end + offset;
            break;
        case SEEK_CUR:
            if (echfs_handle->ptr + offset >= echfs_handle->end
                && !(
                    (
                        ((flags & O_ACCMODE) == O_WRONLY)
                     || ((flags & O_ACCMODE) == O_RDWR)
                    )
                )) {
                goto einval;
            }
            echfs_handle->ptr += offset;
            break;
        default:
        einval:
            spinlock_release(&mnt->lock);
            dynarray_unref(handles, handle);
            errno = EINVAL;
            return -1;
    }

    long ret = echfs_handle->ptr;
    spinlock_release(&mnt->lock);
    dynarray_unref(handles, handle);
    return ret;
}

static int echfs_dup(int handle) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    echfs_handle->refcount++;
    echfs_handle->cached_file->refcount++;

    dynarray_unref(handles, handle);
    return 0;
}

static int echfs_readdir(int handle, struct dirent *dir) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (echfs_handle->type != DIRECTORY_TYPE) {
        dynarray_unref(handles, handle);
        errno = ENOTDIR;
        return -1;
    }

    struct mount_t *mnt = echfs_handle->mnt;

    spinlock_acquire(&mnt->lock);

    struct cached_file_t *cached_file = echfs_handle->cached_file;

    uint64_t dir_id = cached_file->path_res.target.payload;

    struct entry_t entry;
    uint64_t loc = mnt->dirstart * mnt->bytesperblock +
                    echfs_handle->ptr * sizeof(struct entry_t);
    lseek(mnt->device, loc, SEEK_SET);

    for (;;) {
        // check if past directory table
        if (echfs_handle->ptr >= (mnt->dirsize * mnt->entriesperblock)) goto end_of_dir;
        read(mnt->device, (void *)&entry, sizeof(struct entry_t));
        if (!entry.parent_id) goto end_of_dir;              // check if past last entry
        echfs_handle->ptr++;
        if (entry.parent_id == dir_id) {
            // valid entry
            dir->d_ino = echfs_handle->ptr + 1;
            strcpy(dir->d_name, entry.name);
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
    dynarray_unref(handles, handle);
    return 0;

end_of_dir:
    spinlock_release(&mnt->lock);
    dynarray_unref(handles, handle);
    errno = 0;
    return -1;
}

static int echfs_fstat(int handle, struct stat *st) {
    struct echfs_handle_t *echfs_handle =
                dynarray_getelem(struct echfs_handle_t, handles, handle);

    if (!echfs_handle) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = echfs_handle->mnt;

    spinlock_acquire(&mnt->lock);

    struct path_result_t *path_res = &echfs_handle->cached_file->path_res;

    st->st_dev = mnt->device;
    st->st_ino = path_res->target_entry + 1;
    st->st_nlink = 1;
    st->st_uid = path_res->target.owner;
    st->st_gid = path_res->target.group;
    st->st_rdev = 0;
    st->st_size = path_res->target.size;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 512 - 1) / 512;
    st->st_atim.tv_sec = path_res->target.atime;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = path_res->target.mtime;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = path_res->target.ctime;
    st->st_ctim.tv_nsec = 0;

    st->st_mode = 0;
    switch (echfs_handle->type) {
        case DIRECTORY_TYPE:
            st->st_mode |= S_IFDIR;
            break;
        case FILE_TYPE:
            st->st_mode |= S_IFREG;
            break;
    }

    st->st_mode |= path_res->target.perms;

    spinlock_release(&mnt->lock);
    dynarray_unref(handles, handle);
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
    if (strncmp(signature, "_ECH_FS_", 8)) {
        kprint(KPRN_ERR, "echidnaFS signature invalid, mount failed!");
        close(device);
        errno = EINVAL;
        return -1;
    }

    struct mount_t mount;

    mount.device = device;
    strcpy(mount.name, source);
    mount.blocks = rd_qword(device, 12);
    mount.bytesperblock = rd_qword(device, 28);
    mount.sectorsperblock = mount.bytesperblock / BYTES_PER_SECT;
    mount.entriesperblock = mount.sectorsperblock * ENTRIES_PER_SECT;
    mount.fatsize = (mount.blocks * sizeof(uint64_t)) / mount.bytesperblock;
    if ((mount.blocks * sizeof(uint64_t)) % mount.bytesperblock) mount.fatsize++;
    mount.fatstart = RESERVED_BLOCKS;
    mount.dirsize = rd_qword(device, 20);
    mount.dirstart = mount.fatstart + mount.fatsize;
    mount.datastart = RESERVED_BLOCKS + mount.fatsize + mount.dirsize;
    ht_init(mount.cached_files);
    mount.lock = new_lock;

    int ret = dynarray_add(struct mount_t, mounts, &mount);

    return ret;
}

void init_fs_echfs(void) {
    struct fs_t echfs = {0};

    echfs = default_fs_handler;
    strcpy(echfs.name, "echfs");
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
    echfs.unlink = echfs_unlink;
    echfs.mkdir = echfs_mkdir;

    vfs_install_fs(&echfs);
}
