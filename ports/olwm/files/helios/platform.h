/* SPDX-License-Identifier: MIT */
#ifndef HELIOS_OLWM_PLATFORM_H
#define HELIOS_OLWM_PLATFORM_H
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <helios/unix/spawn.h>
#include <helios/unix/poll.h>

#ifndef MAXPID
#define MAXPID 32767
#endif
int olwm_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int olwm_launch(const char *, char **);
int olwm_run_wait(const char *);
void olwm_restart(char **);
#endif
