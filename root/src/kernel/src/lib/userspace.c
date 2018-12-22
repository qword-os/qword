#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <elf.h>

/* TODO expand this to be like execve */
pid_t kexec(const char *filename, const char *argv[], const char *envp[],
        const char *stdin, const char *stdout, const char *stderr) {
    int ret;
    size_t entry;

    /* Create a new pagemap for the process */
    pt_entry_t *pml4 = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);
    if ((size_t)pml4 == MEM_PHYS_OFFSET) return -1;

    struct pagemap_t *pagemap = kalloc(sizeof(struct pagemap_t));
    if (!pagemap) {
        pmm_free((void *)((size_t)pml4 - MEM_PHYS_OFFSET), 1);
        return -1;
    }
    pagemap->pml4 = pml4;
    spinlock_release(&pagemap->lock);

    /* Load the executable */
    int fd = open(filename, 0, 0);
    if (fd == -1) {
        kfree(pagemap);
        pmm_free((void *)((size_t)pml4 - MEM_PHYS_OFFSET), 1);
        return -1;
    }

    struct auxval_t auxval;
    char *ld_path;
    ret = elf_load(fd, pagemap, 0, &auxval, &ld_path);
    close(fd);
    if (ret == -1) {
        kprint(KPRN_DBG, "elf: Load of binary file %s failed.", filename);
        return -1;
    }

    /* If requested: Load the dynamic linker */
    if (!ld_path) {
        entry = auxval.at_entry;
    } else {
        int ld_fd = open(ld_path, 0, 0);
        kfree(ld_path);
        if (ret == -1) {
            kprint(KPRN_DBG, "elf: Could not find dynamic linker.");
            return -1;
        }

        /* 1 GiB is chosen arbitrarily (as programs are expected to fit below 1 GiB).
           TODO: Dynamically find a virtual address range that is large enough */
        struct auxval_t ld_auxval;
        ret = elf_load(ld_fd, pagemap, 0x40000000, &ld_auxval, NULL);
        close(ld_fd);
        if (ret == -1) {
            kprint(KPRN_DBG, "elf: Load of binary file %s failed.", filename);
            return -1;
        }
        entry = ld_auxval.at_entry;
    }

    /* Create a new process */
    pid_t new_pid = task_pcreate(pagemap);
    if (new_pid == (pid_t)(-1)) return -1;

    process_table[new_pid]->file_handles[0] = open(stdin, 0, 0);
    process_table[new_pid]->file_handles[1] = open(stdout, 0, 0);
    process_table[new_pid]->file_handles[2] = open(stderr, 0, 0);

    /* Create main thread */
    tid_t new_thread = task_tcreate(new_pid, tcreate_elf_exec,
            tcreate_elf_exec_data((void *)entry, argv, envp, &auxval));
    if (new_thread == (tid_t)(-1)) return -1;

    return new_pid;
}
