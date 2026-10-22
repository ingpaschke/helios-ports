/* SPDX-License-Identifier: MIT */
#ifndef XVH_TERMINAL_H
#define XVH_TERMINAL_H
#include <sys/ioctl.h>

/* Private commands; the native bridge implements each explicitly. */
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#define TIOCSWINSZ _IOW('t', 103, struct winsize)
#define TIOCGPGRP _IOR('t', 119, int)
#define TIOCSTI _IOW('t', 114, char)
#define TIOCREMOTE _IOW('t', 105, int)

int xvh_terminal_open(int *, int *);
int xvh_terminal_start(char **, unsigned long, char *);
int xvh_terminal_read(int, char *, int);
int xvh_terminal_packet(int, char *, char *, int);
void xvh_terminal_close(void);
long xvh_terminal_ioctl(int, int, char *);
#define ioctl xvh_terminal_ioctl
#endif
