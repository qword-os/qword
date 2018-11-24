#include <stdio.h>

int main(void) {
    for (;;) {
        char prompt[256];
        fprintf(stdout, "qword> ");
        fflush(stdout);
        fgets(prompt, 255, stdin);
        fprintf(stdout, "%s", prompt);
        fflush(stdout);
    }
}
