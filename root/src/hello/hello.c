#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    for (int i = 0; i < argc; i++)
        printf("My %d-th argument is %s\n", i, argv[i]);
    printf("getenv(\"FOO\") returns %s\n", getenv("FOO"));
    for (;;) {
        char prompt[256];
        fprintf(stdout, "qword> ");
        fflush(stdout);
        fgets(prompt, 255, stdin);
        fprintf(stdout, "%s", prompt);
        fflush(stdout);
    }
}
