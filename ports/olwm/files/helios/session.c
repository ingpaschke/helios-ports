/* SPDX-License-Identifier: MIT */
#include "platform.h"
#include <stdio.h>

static pid_t desktop = -1;
static void forward_signal(int sig)
{
    if (desktop > 0) kill(desktop, sig);
}

int main(void)
{
    struct hu_spawn_attr attr = { HU_SPAWN_DEFAULT, 0, 0, 3 };
    char *args[] = { "/helios/bin/x11/olwm", "-single", 0 };
    pid_t result;
    int error, status;
    signal(SIGHUP, forward_signal);
    signal(SIGINT, forward_signal);
    signal(SIGTERM, forward_signal);
    error = hu_spawn(&desktop, args[0], 0, &attr, args, 0);
    if (error) { errno = error; perror("olwm-session: start"); return 1; }
    do { result = waitpid(desktop, &status, 0); }
    while (result < 0 && errno == EINTR);
    desktop = -1;
    if (result < 0) { perror("olwm-session: wait"); return 1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
