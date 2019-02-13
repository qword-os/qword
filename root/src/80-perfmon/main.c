
#include <qword/perfmon.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    puts("hello");

    fflush(stdout);

    char *oops = 0;
    oops[0] = 'a';

    return EXIT_SUCCESS;
}

