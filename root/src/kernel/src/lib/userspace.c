#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <fs.h>
#include <task.h>
#include <lock.h>
#include <klib.h>
#include <elf.h>

#define DEFAULT_STACK_SIZE 32768
#define VIRT_STACK_LOCATION_TOP 0xf0000000

/* TODO expand this to be like execve */
pid_t kexec(const char *filename, const char *argv[], const char *envp[]) {
    /* Create a new pagemap for the process */
    pt_entry_t *pagemap = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);
    if (pagemap == (void *)MEM_PHYS_OFFSET) return -1;

    struct pagemap *new_pagemap = kalloc(sizeof(struct pagemap));
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

    size_t a = VIRT_STACK_LOCATION_TOP - DEFAULT_STACK_SIZE;

    /* Allocate and map stack into the process */
    void *stack = pmm_alloc(DEFAULT_STACK_SIZE / PAGE_SIZE);
    if (!stack) return -1;
    for (size_t i = 0; i < DEFAULT_STACK_SIZE / PAGE_SIZE; i++) {
        map_page(new_pagemap, (size_t)(stack + (i * PAGE_SIZE)),
                    (size_t)(a + (i * PAGE_SIZE)), 0x07);
    }

    spinlock_acquire(scheduler_lock);

    /* Create a new process */
    pid_t new_pid = task_pcreate(new_pagemap);
    if (new_pid == (pid_t)(-1)) return -1;

    /* Create main thread */
    tid_t new_thread = task_tcreate(new_pid, (void *)VIRT_STACK_LOCATION_TOP, (void *)entry, 0);
    if (new_thread == (tid_t)(-1)) return -1;

    spinlock_release(scheduler_lock);

    return new_pid;
}
