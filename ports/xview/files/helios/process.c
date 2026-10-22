/* SPDX-License-Identifier: MIT */
#include <sys/wait.h>
#include <helios/unix/spawn.h>
#include <helios/unix/channel.h>

/* A Helios child need not share memory with its parent. Descriptor actions
 * and hu_spawnp report exec failures without a shared vfork flag. */
int xvh_start_filter(char **arguments, int *input, int *output)
{
    int to[2] = {-1, -1}, from[2] = {-1, -1};
    int error, saved, i;
    pid_t child;
    struct hu_spawn_actions actions;
    struct hu_spawn_attr attr = {HU_SPAWN_LOCAL, NULL, NULL, 3};
    if (!arguments || !arguments[0]) { errno = EINVAL; return -1; }
    if (hu_channel(to) < 0 || hu_channel(from) < 0) goto fail;
    if (fcntl(to[1], F_SETFL, O_NONBLOCK) < 0 ||
        fcntl(from[0], F_SETFL, O_NONBLOCK) < 0) goto fail;
    hu_spawn_actions_init(&actions);
    error = hu_spawn_adddup2(&actions, to[0], 0);
    if (!error) error = hu_spawn_adddup2(&actions, from[1], 1);
    if (!error) error = hu_spawnp(&child, arguments[0], &actions, &attr,
                                arguments, NULL);
    hu_spawn_actions_destroy(&actions);
    if (error) { errno = error; goto fail; }
    close(to[0]); close(from[1]);
    *input = to[1]; *output = from[0];
    return child;
fail:
    saved = errno;
    for (i = 0; i < 2; ++i) {
        if (to[i] >= 0) close(to[i]);
        if (from[i] >= 0) close(from[i]);
    }
    errno = saved;
    return -1;
}
