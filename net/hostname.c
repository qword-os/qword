#include <net/hostname.h>
#include <fd/fd.h>
#include <fd/vfs/vfs.h>
#include <lib/klib.h>

#define DEFAULT_HOSTNAME "qword"

char hostname[1024];

void init_hostname(void) {
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd == -1) {
        kprint(KPRN_WARN, "net/hostname: Failed to open \"/etc/hostname\", using default hostname: %s", DEFAULT_HOSTNAME);
        strcpy(hostname, DEFAULT_HOSTNAME);
        return;
    }
    read(fd, hostname, 1023);
    for (int i = 0; i < 1024; i++) {
        if (hostname[i] == '\n') {
            hostname[i] = 0;
            break;
        }
    }
    hostname[1023] = 0;
    close(fd);
    kprint(KPRN_INFO, "net/hostname: Hostname set to \"%s\"", hostname);
}
