#include <stddef.h>
#include <lib/klib.h>
#include <lib/cmdline.h>

static char value[256];

char *cmdline_get_value(const char *key) {
    size_t key_len = strlen(key);

    for (size_t i = 0; cmdline[i]; i++) {
        if (!strncmp(&cmdline[i], key, key_len) && cmdline[i + key_len] == '=') {
            if (i && cmdline[i - 1] != ' ')
                continue;
            i += key_len + 1;
            size_t j;
            for (j = 0; cmdline[i + j] != ' ' && cmdline[i + j]; j++) {
                value[j] = cmdline[i + j];
            }
            value[j] = 0;
            return value;
        }
    }

    return (char *)0;
}
