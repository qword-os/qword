#include <net/hostname.h>
#include <fd/fd.h>
#include <fd/vfs/vfs.h>
#include <lib/klib.h>
#include <lib/cstring.h>

#define DEFAULT_HOSTNAME "qword"

char hostname[MAX_HOSTNAME_LEN];

void init_hostname(void) {
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd == -1) {
        kprint(KPRN_WARN, "net/hostname: Failed to open \"/etc/hostname\", using default hostname: %s", DEFAULT_HOSTNAME);
        strcpy(hostname, DEFAULT_HOSTNAME);
        return;
    }
    read(fd, hostname, MAX_HOSTNAME_LEN-1);
    for (int i = 0; i < MAX_HOSTNAME_LEN; i++) {
        if (hostname[i] == '\n') {
            hostname[i] = 0;
            break;
        }
    }
    hostname[MAX_HOSTNAME_LEN-1] = 0;
    close(fd);
    kprint(KPRN_INFO, "net/hostname: Hostname set to \"%s\"", hostname);
}
