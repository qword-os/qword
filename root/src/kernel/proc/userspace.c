#include <stdint.h>
#include <stddef.h>
#include <mm/mm.h>
#include <fd/vfs/vfs.h>
#include <proc/task.h>
#include <lib/klib.h>
#include <proc/elf.h>
#include <lib/lock.h>
#include <lib/event.h>
#include <sys/panic.h>
#include <fd/perfmon/perfmon.h>
#include <lib/signal.h>

extern void *signal_trampoline[];
extern void *signal_trampoline_size[];

int exec(pid_t pid, const char *filename, const char *argv[], const char *envp[]) {
    int ret;
    size_t entry;

    struct process_t *process = process_table[pid];

    struct pagemap_t *old_pagemap = process->pagemap;
    struct pagemap_t *new_pagemap = new_address_space();

    /* Load the executable */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        free_address_space(new_pagemap);
        return -1;
    }

    struct auxval_t auxval;
    char *ld_path;

    ret = elf_load(fd, new_pagemap, 0, &auxval, &ld_path);
    close(fd);
    if (ret == -1) {
        kprint(KPRN_DBG, "elf: Load of binary file %s failed.", filename);
        free_address_space(new_pagemap);
        return -1;
    }

    /* If requested: Load the dynamic linker */
    if (!ld_path) {
        entry = auxval.at_entry;
    } else {
        int ld_fd = open(ld_path, O_RDONLY);
        if (ld_fd < 0) {
            kprint(KPRN_DBG, "elf: Could not find dynamic linker.");
            free_address_space(new_pagemap);
            kfree(ld_path);
            return -1;
        }

        /* 1 GiB is chosen arbitrarily (as programs are expected to fit below 1 GiB).
           TODO: Dynamically find a virtual address range that is large enough */
        struct auxval_t ld_auxval;
        ret = elf_load(ld_fd, new_pagemap, 0x40000000, &ld_auxval, NULL);
        close(ld_fd);
        if (ret == -1) {
            kprint(KPRN_DBG, "elf: Load of binary file %s failed.", ld_path);
            free_address_space(new_pagemap);
            kfree(ld_path);
            return -1;
        }
        kfree(ld_path);
        entry = ld_auxval.at_entry;
    }

    /* Map the higher half into the process */
    for (size_t i = 256; i < 512; i++) {
        new_pagemap->pml4[i] = process_table[0]->pagemap->pml4[i];
    }

    /* Destroy all previous threads */
    for (size_t i = 0; i < MAX_THREADS; i++)
        task_tkill(pid, i);

    // Map the sig ret trampoline into process
    void *trampoline_ptr = pmm_allocz(1);
    memcpy(trampoline_ptr + MEM_PHYS_OFFSET,
            signal_trampoline,
            (size_t)signal_trampoline_size);
    map_page(new_pagemap,
             (size_t)trampoline_ptr,
             (size_t)(SIGNAL_TRAMPOLINE_VADDR),
             0x05,
             VMM_ATTR_REG);

    /* Free previous address space */
    free_address_space(old_pagemap);

    /* Load new pagemap */
    process->pagemap = new_pagemap;

    /* Create main thread */
    task_tcreate(pid, tcreate_elf_exec, tcreate_elf_exec_data((void *)entry, argv, envp, &auxval));

    return 0;
}

pid_t kexec(const char *filename, const char *argv[], const char *envp[],
        const char *stdin, const char *stdout, const char *stderr) {

    /* Create a new process */
    pid_t new_pid = task_pcreate();
    if (new_pid == (pid_t)(-1)) return -1;

    process_table[new_pid]->ppid = 0;

    /* Open stdio descriptors */
    process_table[new_pid]->file_handles[0] = open(stdin, O_RDONLY);
    process_table[new_pid]->file_handles[1] = open(stdout, O_WRONLY);
    process_table[new_pid]->file_handles[2] = open(stderr, O_WRONLY);

    exec(new_pid, filename, argv, envp);

    return new_pid;
}
