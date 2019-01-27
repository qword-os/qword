#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stddef.h>
#include <mm/mm.h>
#include <lib/lock.h>
#include <fd/perfmon/perfmon.h>
#include <lib/time.h>

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

typedef lock_t event_t;

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
    tid_t task_id;
    pid_t process;
    lock_t lock;
    uint64_t yield_target;
    event_t *event_ptr;
    int active_on_cpu;
    uint64_t syscall_entry_time;
    int64_t total_cputime;
    int64_t accounted_cputime;
    size_t kstack;
    size_t ustack;
    size_t errno;
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

struct child_event_t {
    pid_t pid;
    int status;
};

struct process_t {
    pid_t pid;
    pid_t ppid;
    struct pagemap_t *pagemap;
    struct thread_t **threads;
    char cwd[2048];
    lock_t cwd_lock;
    int *file_handles;
    lock_t file_handles_lock;
    size_t cur_brk;
    lock_t cur_brk_lock;
    struct child_event_t *child_events;
    size_t child_event_i;
    lock_t child_event_lock;
    event_t child_event;
    lock_t perfmon_lock;
    struct perfmon_t *active_perfmon;
    lock_t usage_lock;
    struct rusage_t own_usage;
    struct rusage_t child_usage;
};

int task_send_child_event(pid_t, struct child_event_t *);
void task_await_event(event_t *);
void task_trigger_event(event_t *);

extern int64_t task_count;

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
pid_t task_pcreate(void);
int task_tkill(pid_t, tid_t);

#endif
