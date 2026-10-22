/* SPDX-License-Identifier: MIT */
/* File identity on native Helios, whose POSIX stat has no inode numbers. */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <syslib.h>
#include <nonansi.h>

int xvh_same_file(int source, const char *destination)
{
    Stream *from = fdstream(source), *to;
    int fd, same;
    if (!from) { errno = EBADF; return -1; }
    fd = open(destination, O_RDONLY);
    if (fd < 0) return errno == ENOENT ? 0 : -1;
    to = fdstream(fd);
    if (!to) { close(fd); errno = EBADF; return -1; }
    /* Open resolves native links before returning the stream name. */
    same = strcmp(from->Name, to->Name) == 0;
    close(fd);
    return same;
}
