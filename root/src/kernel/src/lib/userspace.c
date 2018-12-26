#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <elf.h>
#include <lock.h>

struct execve_request_t {
    pid_t pid;
    char *filename;
    char **argv;
    char **envp;
};

static size_t execve_requests_i = 0;
static struct execve_request_t *execve_requests = 0;
static lock_t execve_request_lock = 1;

void execve_send_request(pid_t pid, const char *filename, const char **argv, const char **envp) {
    spinlock_acquire(&scheduler_lock);
    spinlock_acquire(&execve_request_lock);

    execve_requests_i++;
    execve_requests = krealloc(execve_requests, sizeof(struct execve_request_t) * execve_requests_i);

    struct execve_request_t *execve_request = &execve_requests[execve_requests_i - 1];

    execve_request->pid = pid;

    execve_request->filename = kalloc(kstrlen(filename) + 1);
    kstrcpy(execve_request->filename, filename);

    size_t argv_i;
    for (argv_i = 0; argv[argv_i]; argv_i++);
    argv_i++;
    execve_request->argv = kalloc(sizeof(char *) * argv_i);
    for (size_t i = 0; ; i++) {
        if (!argv[i]) {
            execve_request->argv[i] = 0;
            break;
        }
        execve_request->argv[i] = kalloc(kstrlen(argv[i]) + 1);
        kstrcpy(execve_request->argv[i], argv[i]);
    }

    size_t envp_i;
    for (envp_i = 0; envp[envp_i]; envp_i++);
    envp_i++;
    execve_request->envp = kalloc(sizeof(char *) * envp_i);
    for (size_t i = 0; ; i++) {
        if (!envp[i]) {
            execve_request->envp[i] = 0;
            break;
        }
        execve_request->envp[i] = kalloc(kstrlen(envp[i]) + 1);
        kstrcpy(execve_request->envp[i], envp[i]);
    }

    spinlock_release(&execve_request_lock);
    spinlock_release(&scheduler_lock);
}

void execve_request_monitor(void *arg) {
    (void)arg;

    kprint(0, "kernel: execve request monitor launched.");

    /* main event loop */
    for (;;) {
        spinlock_acquire(&execve_request_lock);
        if (execve_requests_i) {
            kprint(0, "execve_monitor: executing execve request");
            exec(
                execve_requests[0].pid,
                execve_requests[0].filename,
                (const char **)execve_requests[0].argv,
                (const char **)execve_requests[0].envp
            );

            /* free request mem */

            kfree(execve_requests[0].filename);

            for (size_t i = 0; ; i++) {
                if (!execve_requests[0].argv[i])
                    break;
                kfree(execve_requests[0].argv[i]);
            }
            kfree(execve_requests[0].argv);

            for (size_t i = 0; ; i++) {
                if (!execve_requests[0].envp[i])
                    break;
                kfree(execve_requests[0].envp[i]);
            }
            kfree(execve_requests[0].envp);

            execve_requests_i--;
            for (size_t i = 0; i < execve_requests_i; i++)
                execve_requests[i] = execve_requests[i + 1];
            execve_requests = krealloc(execve_requests, sizeof(struct execve_request_t) * execve_requests_i);
        }
        spinlock_release(&execve_request_lock);
        ksleep(10);
    }
}

int exec(pid_t pid, const char *filename, const char *argv[], const char *envp[]) {
    int ret;
    size_t entry;

    struct process_t *process = process_table[pid];

    struct pagemap_t *old_address_space = process->pagemap;

    /* Destroy all previous threads */
    for (size_t i = 0; i < MAX_THREADS; i++)
        task_tkill(pid, i);

    /* Free previous address space */
    free_address_space(old_address_space);

    /* Load the executable */
    int fd = open(filename, 0, 0);
    if (fd < 0)
        return -1;

    struct auxval_t auxval;
    char *ld_path;

    ret = elf_load(fd, process->pagemap, 0, &auxval, &ld_path);
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
        if (ld_fd < 0) {
            kprint(KPRN_DBG, "elf: Could not find dynamic linker.");
            return -1;
        }

        /* 1 GiB is chosen arbitrarily (as programs are expected to fit below 1 GiB).
           TODO: Dynamically find a virtual address range that is large enough */
        struct auxval_t ld_auxval;
        ret = elf_load(ld_fd, process->pagemap, 0x40000000, &ld_auxval, NULL);
        close(ld_fd);
        if (ret == -1) {
            kprint(KPRN_DBG, "elf: Load of binary file %s failed.", ld_path);
            kfree(ld_path);
            return -1;
        }
        kfree(ld_path);
        entry = ld_auxval.at_entry;
    }

    /* Map the higher half into the process */
    for (size_t i = 256; i < 512; i++) {
        process->pagemap->pml4[i] = process_table[0]->pagemap->pml4[i];
    }

    /* Create main thread */
    tid_t new_thread = task_tcreate(pid, tcreate_elf_exec,
            tcreate_elf_exec_data((void *)entry, argv, envp, &auxval));
    if (new_thread == (tid_t)(-1)) return -1;

    return 0;
}

pid_t kexec(const char *filename, const char *argv[], const char *envp[],
        const char *stdin, const char *stdout, const char *stderr) {

    /* Create a new process */
    pid_t new_pid = task_pcreate();
    if (new_pid == (pid_t)(-1)) return -1;

    /* Open stdio descriptors */
    process_table[new_pid]->file_handles[0] = open(stdin, 0, 0);
    process_table[new_pid]->file_handles[1] = open(stdout, 0, 0);
    process_table[new_pid]->file_handles[2] = open(stderr, 0, 0);

    exec(new_pid, filename, argv, envp);

    return new_pid;
}
