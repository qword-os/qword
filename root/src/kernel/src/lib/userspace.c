#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <elf.h>

/* TODO expand this to be like execve */
pid_t kexec(const char *filename, const char *argv[], const char *envp[]) {
    /* Create a new pagemap for the process */
    pt_entry_t *pagemap = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);
    if (pagemap == (void *)MEM_PHYS_OFFSET) return -1;

    struct pagemap_t *new_pagemap = kalloc(sizeof(struct pagemap_t));
    if (!new_pagemap) return -1;
    new_pagemap->pagemap = pagemap;
    new_pagemap->lock = 1;

    int fd = open(filename, 0, 0);
    if (fd == -1) return -1;

    uint64_t entry;
    int ret = elf_load(fd, new_pagemap, &entry);
    //close(fd);        TODO: echfs_close should be a thing tbh
    if (ret == -1) {
        kprint(KPRN_DBG, "elf: Load of binary file %s failed.", filename);
        return -1;
    }
    kprint(KPRN_DBG, "elf: %s successfully loaded. entry point: %X", filename, entry);

    /* Create a new process */
    pid_t new_pid = task_pcreate(new_pagemap);
    if (new_pid == (pid_t)(-1)) return -1;

    /* Create main thread */
    tid_t new_thread = task_tcreate(new_pid, (void *)entry, 0);
    if (new_thread == (tid_t)(-1)) return -1;

    return new_pid;
}
