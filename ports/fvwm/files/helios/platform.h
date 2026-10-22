/* Native Helios ABI. Do not include a newlib libc or emulate fork. */
#ifndef KTM_FVWM_PLATFORM_H
#define KTM_FVWM_PLATFORM_H
#include "x11-platform.h"
#include <errno.h>
#include <sys/wait.h>
#include <helios/unix/spawn.h>
#include <helios/unix/channel.h>
#include <helios/unix/poll.h>
#include <X11/Xlib.h>
#define O_NDELAY O_NONBLOCK
struct itimerval { struct timeval it_interval, it_value; };
int ktm_read_full(int, void *, unsigned);
int ktm_write_full(int, const void *, unsigned);
void ktm_exec(const char *);
void ktm_restart(const char *, char **);
int ktm_putenv(char *);
int ktm_select(int,fd_set *,fd_set *,fd_set *,struct timeval *);
int ktm_grab_key(Display *,int,unsigned,Window,Bool,int,int);
int ktm_grab_button(Display *,unsigned,unsigned,Window,Bool,unsigned,int,int,Window,Cursor);
int ktm_ungrab_button(Display *,unsigned,unsigned,Window);
#endif
