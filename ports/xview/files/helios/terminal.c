/* SPDX-License-Identifier: MIT */
/* Reuse the separately compiled native backend supplied by the X11 port. */
#define XVH_NATIVE_TERMINAL
#include <termios.h>
#define HELIOS_PTY_TERM "sun"
#include <clients/xterm/helios-pty.c>
#include "terminal.h"
#undef ioctl

int xvh_terminal_open(int *master, int *slave)
{
    int fd;
    /* The shared backend owns one terminal per client process. */
    if (master_fd >= 0) { errno = EBUSY; return -1; }
    fd = helios_pty_open(80, 24);
    if (fd < 0) { perror("Open Look terminal creation"); return -1; }
    *master = fd;
    *slave = slave_fd;
    return 0;
}

int xvh_terminal_start(char **command, unsigned long window, char *display)
{
    if (command && !command[0]) command = NULL;
    return helios_pty_spawn(command, window, display, "sun");
}

int xvh_terminal_read(int fd, char *buffer, int size)
{
    int result;
    if (fd != master_fd) return read(fd, buffer, size);
    if (size < 3) { errno = EINVAL; return -1; }
    /* XView expects the packet byte and seven-bit control sequences. */
    result = helios_pty_read(fd, buffer + 1, size - 1);
    if (result <= 0) return result;
    buffer[0] = 0;
    return result + 1;
}

/* Native master reads contain a packet header. readv() cannot be emulated by
 * two reads: each native read starts a new packet and can block independently.
 * Read one packet and hand its control byte and payload to XView together. */
int xvh_terminal_packet(int fd, char *control, char *data, int capacity)
{
    int count;
    if (fd != master_fd || !control || !data || capacity < 2) {
        errno = EINVAL;
        return -1;
    }
    count = helios_pty_read(fd, data, capacity);
    if (count <= 0) return count;
    *control = 0;
    return count + 1;
}

void xvh_terminal_close(void)
{
    helios_pty_close();
}

long xvh_terminal_ioctl(int fd, int command, char *argument)
{
    switch (command) {
    case TIOCSWINSZ: {
        struct winsize *size = (struct winsize *)argument;
        if (!size || !size->ws_col || !size->ws_row) { errno = EINVAL; return -1; }
        Attributes attributes;
        if (fd != master_fd && fd != slave_fd) { errno = EBADF; return -1; }
        if (GetAttributes(fdstream(master_fd), &attributes) < 0) {
            errno = EIO; return -1;
        }
        attributes.Time = size->ws_col;
        attributes.Min = size->ws_row;
        if (SetAttributes(fdstream(master_fd), &attributes) < 0) {
            errno = EIO; return -1;
        }
        return 0;
    }
    case TIOCGPGRP:
        if (!argument) { errno = EINVAL; return -1; }
        *(int *)argument = tcgetpgrp(slave_fd);
        return *(int *)argument < 0 ? -1 : 0;
    case TIOCSTI:
        return write(master_fd, argument, 1) == 1 ? 0 : -1;
    case TIOCREMOTE:
        if (!argument) { errno = EINVAL; return -1; }
        /* Ordinary mode is already active. Record mode is not available. */
        if (!*(int *)argument) return 0;
        errno = EOPNOTSUPP;
        return -1;
    default:
        return ioctl(fd, command, argument);
    }
}

int updateutmp(char *user, int slot, int fd)
{
    (void)user; (void)slot; (void)fd;
    return 0;
}
