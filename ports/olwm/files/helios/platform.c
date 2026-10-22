/* SPDX-License-Identifier: MIT */
#include "platform.h"
#include <stdio.h>
#include <limits.h>

int olwm_select(int width, fd_set *in, fd_set *out, fd_set *except,
                struct timeval *tv)
{
    struct hu_pollfd fds[FD_SETSIZE];
    int i, count = 0, result, ready = 0, timeout = -1;
    if (width < 0 || width > FD_SETSIZE) { errno = EINVAL; return -1; }
    if (tv) {
        if (tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000) {
            errno = EINVAL; return -1;
        }
        timeout = tv->tv_sec >= (INT_MAX - 1000) / 1000 ? INT_MAX :
                  tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000;
    }
    for (i = 0; i < width; ++i) {
        int events = 0;
        if (in && FD_ISSET(i, in)) events |= HU_POLLIN;
        if (out && FD_ISSET(i, out)) events |= HU_POLLOUT;
        if (except && FD_ISSET(i, except)) events |= HU_POLLPRI;
        if (events) {
            fds[count].fd = i;
            fds[count].events = events;
            fds[count++].revents = 0;
        }
    }
    result = hu_poll(fds, count, timeout);
    if (result < 0) return -1;
    if (in) FD_ZERO(in);
    if (out) FD_ZERO(out);
    if (except) FD_ZERO(except);
    for (i = 0; i < count; ++i) {
        int re = fds[i].revents, events = fds[i].events;
        if (re & HU_POLLNVAL) { errno = EBADF; return -1; }
        if ((events & HU_POLLIN) && (re & (HU_POLLIN | HU_POLLHUP | HU_POLLERR))) {
            FD_SET(fds[i].fd, in); ++ready;
        }
        if ((events & HU_POLLOUT) && (re & (HU_POLLOUT | HU_POLLHUP | HU_POLLERR))) {
            FD_SET(fds[i].fd, out); ++ready;
        }
        if ((events & HU_POLLPRI) && (re & HU_POLLPRI)) {
            FD_SET(fds[i].fd, except); ++ready;
        }
    }
    return ready;
}

static int launch(const char *command, char **environment, pid_t *child)
{
    struct hu_spawn_attr attr = { HU_SPAWN_DEFAULT, 0, 0, 3 };
    char *args[] = { "/helios/bin/shell", "-f", "-c", (char *)command, 0 };
    int error = hu_spawn(child, args[0], 0, &attr, args, environment);
    if (error) { errno = error; perror("olwm: launch"); return -1; }
    return 0;
}

int olwm_launch(const char *command, char **environment)
{
    pid_t child;
    return launch(command, environment, &child) < 0;
}

int olwm_run_wait(const char *command)
{
    pid_t child, result;
    int status;
    if (launch(command, 0, &child) < 0) return -1;
    do { result = waitpid(child, &status, 0); }
    while (result < 0 && errno == EINTR);
    return result < 0 ? -1 : status;
}

void olwm_restart(char **args)
{
    struct hu_spawn_attr attr = { HU_SPAWN_LOCAL, 0, 0, 3 };
    pid_t child, result;
    int error, status;
    error = hu_spawnp(&child, args[0], 0, &attr, args, 0);
    if (error) { errno = error; perror("olwm: restart"); exit(1); }
    /* Keep the original TFM session owner alive until its replacement exits. */
    do { result = waitpid(child, &status, 0); }
    while (result < 0 && errno == EINTR);
    exit(result >= 0 && WIFEXITED(status) ? WEXITSTATUS(status) : 1);
}
