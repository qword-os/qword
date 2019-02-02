
#include <qword/perfmon.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "perfmon usage: perfmon COMMAND [ARGUMENTS...]\n");
        exit(EXIT_FAILURE);
    }

    int pm_fd = perfmon_create();
    if (pm_fd < 0) {
        fprintf(stderr, "perfmon: Failed to create perfmon FD. Error: %m\n");
        exit(EXIT_FAILURE);
    }

    int child = fork();
    if (child < 0) {
        fprintf(stderr, "perfmon: fork() failed. Error: %m\n");
        exit(EXIT_FAILURE);
    }
    if (!child) {
        if (perfmon_attach(pm_fd)) {
            fprintf(stderr, "perfmon: Could not attach perfmon to process. Error: %m\n");
            exit(EXIT_FAILURE);
        }

        execve(argv[1], argv + 1, environ);
        fprintf(stderr, "perfmon: execve() failed in child. Error: %m\n");
        exit(EXIT_FAILURE);
    }

    if (waitpid(child, NULL, 0) < 0) {
        fprintf(stderr, "perfmon: waitpid() failed. Error: %m\n");
        exit(EXIT_FAILURE);
    }

    struct perfstats ps;
    size_t read_len = read(pm_fd, &ps, sizeof(struct perfstats));
    if (read_len < 0) {
        fprintf(stderr, "perfmon: Reading perfmon FD failed. Error: %m\n");
        exit(EXIT_FAILURE);
    }
    if (read_len != sizeof(struct perfstats)) {
        fprintf(stderr, "perfmon: Reading perfmon FD returned unexpected size\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Total CPU time: %d\n", ps.cpu_time);
    fprintf(stderr, "Time spent in syscalls: %d\n", ps.syscall_time);
    fprintf(stderr, "Time spent in MM: %d\n", ps.mman_time);
    fprintf(stderr, "Time spent in I/O: %d\n", ps.io_time);

    return EXIT_SUCCESS;
}

