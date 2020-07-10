#include <stddef.h>
#include <lib/klib.h>
#include <lib/cmdline.h>
#include <lib/cstring.h>

static const char *cmdline;

void init_cmdline(const char *cmd) {
    cmdline = cmd;
}

char *cmdline_get_value(char *buf, size_t limit, const char *key) {
    if (!limit || !buf)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; cmdline[i]; i++) {
        if (!strncmp(&cmdline[i], key, key_len) && cmdline[i + key_len] == '=') {
            if (i && cmdline[i - 1] != ' ')
                continue;
            i += key_len + 1;
            size_t j;
            for (j = 0; cmdline[i + j] != ' ' && cmdline[i + j]; j++) {
                if (j == limit - 1)
                    break;
                buf[j] = cmdline[i + j];
            }
            buf[j] = 0;
            return buf;
        }
    }

    return NULL;
}
