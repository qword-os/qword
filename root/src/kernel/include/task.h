#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <lock.h>

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define MAX_TASKS (MAX_PROCESSES*16)
#define MAX_FILE_HANDLES 256

#define CURRENT_PROCESS cpu_locals[current_cpu].current_process
#define CURRENT_THREAD cpu_locals[current_cpu].current_thread

#define fxsave(PTR) ({ \
    asm volatile ("fxsave [rbx];" : : "b" (PTR)); \
})

#define fxrstor(PTR) ({ \
    asm volatile ("fxrstor [rbx];" : : "b" (PTR)); \
})

#define load_fs_base(PTR) ({ \
    asm volatile ( \
        "mov rcx, 0xc0000100;" \
        "mov eax, edx;" \
        "shr rdx, 32;" \
        "mov edx, edx;" \
        "wrmsr;" \
        : \
        : "d" (PTR) \
        : "rax", "rcx" \
    ); \
})

struct ctx_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef int32_t pid_t;
typedef int32_t tid_t;
typedef int32_t uid_t;
typedef int32_t gid_t;

struct thread_t {
    tid_t tid;
    pid_t process;
    lock_t lock;
    uint64_t yield_target;
    int active_on_cpu;
    size_t kstack;
    size_t ustack;
    size_t fs_base;
    struct ctx_t ctx;
    uint8_t fxstate[512] __attribute__((aligned(16)));
};

#define AT_ENTRY 10
#define AT_PHDR 20
#define AT_PHENT 21
#define AT_PHNUM 22

struct auxval_t {
    size_t at_entry;
    size_t at_phdr;
    size_t at_phent;
    size_t at_phnum;
};

struct process_t {
    pid_t pid;
    int priority;
    struct pagemap_t *pagemap;
    struct thread_t **threads;
    char cwd[2048];
    int *file_handles;
    size_t cur_brk;
    struct auxval_t auxval;
};

extern lock_t scheduler_lock;

extern struct process_t **process_table;
extern struct thread_t **task_table;

void init_sched(void);

void yield(uint64_t);

enum tcreate_abi {
	tcreate_fn_call,
	tcreate_elf_exec
};

struct tcreate_fn_call_data{
    void (*fn)(void *);
    void *arg;
};

struct tcreate_elf_exec_data {
    void *entry;
    const char **argv;
    const char **envp;
    const struct auxval_t *auxval;
};

#define tcreate_fn_call_data(fn_, arg_) \
    &((struct tcreate_fn_call_data){.fn=fn_, .arg=arg_})
#define tcreate_elf_exec_data(entry_, argv_, envp_, auxval_) \
    &((struct tcreate_elf_exec_data){.entry=entry_, .argv=argv_, .envp=envp_, .auxval=auxval_})

tid_t task_tcreate(pid_t, enum tcreate_abi, const void *);
pid_t task_pcreate(struct pagemap_t *);
int task_tkill(pid_t, tid_t);

#endif
