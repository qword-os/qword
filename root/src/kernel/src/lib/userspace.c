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
    pt_entry_t *pml4 = (pt_entry_t *)((size_t)pmm_alloc(1) + MEM_PHYS_OFFSET);
    if ((size_t)pml4 == MEM_PHYS_OFFSET) return -1;

    struct pagemap_t *pagemap = kalloc(sizeof(struct pagemap_t));
    if (!pagemap) {
        pmm_free((void *)((size_t)pml4 - MEM_PHYS_OFFSET), 1);
        return -1;
    }
    pagemap->pml4 = pml4;
    spinlock_release(&pagemap->lock);

    int fd = open(filename, 0, 0);
    if (fd == -1) {
        kfree(pagemap);
        pmm_free((void *)((size_t)pml4 - MEM_PHYS_OFFSET), 1);
        return -1;
    }

    struct auxval_t auxval;
    int ret = elf_load(fd, pagemap, &auxval);
    close(fd);
    if (ret == -1) {
        kprint(KPRN_DBG, "elf: Load of binary file %s failed.", filename);
        return -1;
    }
    kprint(KPRN_DBG, "elf: %s successfully loaded.", filename);
    kprint(KPRN_DBG, "AT_ENTRY: %X", auxval.at_entry);
    kprint(KPRN_DBG, "AT_PHDR: %X", auxval.at_phdr);
    kprint(KPRN_DBG, "AT_PHENT: %X", auxval.at_phent);
    kprint(KPRN_DBG, "AT_PHNUM: %X", auxval.at_phnum);

    /* Create a new process */
    pid_t new_pid = task_pcreate(pagemap);
    if (new_pid == (pid_t)(-1)) return -1;

    process_table[new_pid]->auxval = auxval;

    /* Create main thread */
    tid_t new_thread = task_tcreate(new_pid, (void *)auxval.at_entry, 0);
    if (new_thread == (tid_t)(-1)) return -1;

    return new_pid;
}
