#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#define SIGNAL_MAX 128

#define SIGNAL_TRAMPOLINE_VADDR ((size_t)0x0000740000000000)

typedef long sigset_t;

// from: mlibc/options/ansi/include/signal.h

#define SIG_ERR (void *)(-1)
#define SIG_DFL (void *)(-2)
#define SIG_IGN (void *)(-3)

#define SIGABRT 1
#define SIGFPE 2
#define SIGILL 3
#define SIGINT 4
#define SIGSEGV 5
#define SIGTERM 6
#define SIGPROF 7
#define SIGIO 9
#define SIGPWR 10
#define SIGRTMIN 11
#define SIGRTMAX 12

__attribute__((unused)) static const char *signames[] = {
    "0",
    "SIGABRT",
    "SIGFPE",
    "SIGILL",
    "SIGINT",
    "SIGSEGV",
    "SIGTERM",
    "SIGPROF",
    "8",
    "SIGIO",
    "SIGPWR",
    "SIGRTMIN",
    "SIGRTMAX"
};

#endif
