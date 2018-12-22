#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    for (int i = 0; i < argc; i++)
        printf("My %d-th argument is %s\n", i, argv[i]);
    printf("getenv(\"FOO\") returns %s\n", getenv("FOO"));
    for (;;) {
        char prompt[256];
        fprintf(stdout, "%d: qword> ", getpid());
        fflush(stdout);
        fgets(prompt, 255, stdin);
        prompt[strlen(prompt)-1] = 0;
        fprintf(stdout, "%s\n", prompt);
        fflush(stdout);
        if (!strcmp(prompt, "fork")) {
            if (!fork()) {
                printf("child process. pid: %d\n", getpid());
                fflush(stdout);
            } else {
                printf("parent process. pid: %d\n", getpid());
                fflush(stdout);
            }
        } else if (!strcmp(prompt, "pid")) {
            printf("%d\n", getpid());
        }
    }
}
