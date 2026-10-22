/* The TFM's first task owns the native session lifetime. XDM's local
 * shells must start FVWM through that TFM before any short-lived helpers. */
#include "platform.h"
#include <stdio.h>
#include <signal.h>

static pid_t desktop = -1;

static void forward_signal(int sig)
{
    if (desktop > 0) kill(desktop, sig);
}

int main(int argc, char **argv)
{
    struct hu_spawn_attr attr = { HU_SPAWN_DEFAULT, 0, 0, 3 };
    char *args[] = { "/helios/bin/x11/fvwm", "-s", 0 };
    int error, status;
    pid_t result;
    (void)argc; (void)argv;
    signal(SIGHUP, forward_signal);
    signal(SIGTERM, forward_signal);
    signal(SIGINT, forward_signal);
    error = hu_spawn(&desktop, args[0], 0, &attr, args, 0);
    if (error) {
        errno = error;
        perror("fvwm-session: start desktop through TFM");
        return 1;
    }
    do { result = waitpid(desktop, &status, 0); }
    while (result < 0 && errno == EINTR);
    desktop = -1;
    if (result < 0) { perror("fvwm-session: wait"); return 1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
