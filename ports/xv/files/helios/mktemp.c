/* SPDX-License-Identifier: MIT
 * XV's private temporary-name adapter. Reserve each returned name on disk.
 */
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef helios
#include <syslib.h>
#endif

static unsigned long mktemp_counter;

char *xv_mktemp(char *template)
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    unsigned long seed, value;
    size_t length;
    int attempt, digit, fd, error;
#ifdef helios
    word port = 0;
    char machine[512], *p;
#endif

    length = strlen(template);
    if (length < 6 || strcmp(template + length - 6, "XXXXXX") != 0) {
        template[0] = '\0';
        errno = EINVAL;
        return template;
    }

    seed = (unsigned long) time((time_t *) 0);
#ifdef helios
    /* POSIX getpid() starts at 42 in each task. Native ports distinguish
     * tasks, and the machine name distinguishes ports on different CPUs. */
    machine[0] = '\0';
    TaskData(TD_Port, &port);
    MachineName(machine);
    for (p = machine; *p; p++) seed = seed * 33UL + (unsigned char) *p;
    seed ^= (unsigned long) port;
#else
    seed ^= (unsigned long) getpid();
#endif
    seed += ++mktemp_counter;
    for (attempt = 0; attempt < 100; attempt++) {
        value = seed + (unsigned long) attempt * 7919UL;
        for (digit = 0; digit < 6; digit++) {
            template[length - 6 + digit] = alphabet[value % 36];
            value /= 36;
        }
        fd = open(template, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            if (close(fd) == 0) return template;
            error = errno;
            unlink(template);
            errno = error;
            break;
        }
        if (errno != EEXIST) break;
    }
    template[0] = '\0';
    return template;
}
