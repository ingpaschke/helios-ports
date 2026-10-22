/* SPDX-License-Identifier: MIT */
#ifndef HELIOS_XVIEW_PLATFORM_H
#define HELIOS_XVIEW_PLATFORM_H
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <locale.h>
#include <X11/Xlib.h>

#ifndef LC_MESSAGES
#define LC_MESSAGES LC_ALL
#endif
#ifndef O_NDELAY
#define O_NDELAY O_NONBLOCK
#endif
#ifndef MAXSYMLINKS
#define MAXSYMLINKS 20
#endif
#ifndef ICANON
#define ICANON ICANNON
#endif

#ifndef NFDBITS
#define NFDBITS (8 * sizeof(long))
#endif
#ifndef NBBY
#define NBBY 8
#endif
#ifndef howmany
#define howmany(n, size) (((n) + (size) - 1) / (size))
#endif
#ifndef __va_copy
#define __va_copy(dst, src) ((dst)[0] = (src)[0])
#endif
#ifndef FNDELAY
#define FNDELAY O_NDELAY
#endif
#ifndef MAXNAMLEN
#define MAXNAMLEN 511
#endif

#include <xview/helios.h>
int xvh_getitimer(int, struct itimerval *);
int xvh_setitimer(int, const struct itimerval *, struct itimerval *);
#define getitimer xvh_getitimer
#define setitimer xvh_setitimer

#include <helios/unix/channel.h>
#define pipe hu_channel
/* Keep XView's notifier wrappers private to XView, leaving libc/Xlib alone. */
#define read xvh_read
#define fcntl xvh_fcntl
#define select xvh_select
int xvh_read();
int xvh_fcntl(int, int, ...);
int xvh_select();
char *xvh_strdup(const char *);
char *xvh_getcwd(char *, int);
#define getcwd xvh_getcwd
int xvh_killpg(int, int);
int xvh_same_file(int, const char *);
int xvh_start_filter(char **, int *, int *);
int xvh_format_strings(char *, unsigned, const char *, ...);
double xvh_round_pixel(double);
#define strdup xvh_strdup
#define killpg xvh_killpg
#endif
