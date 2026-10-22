/* SPDX-License-Identifier: MIT */
#ifndef XVH_NOTIFY_TIME_H
#define XVH_NOTIFY_TIME_H
#include <sys/time.h>
/* Helios supplies timeval but has no interval-timer ABI. */
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define timercmp(a,b,op) (((a)->tv_sec == (b)->tv_sec) ? \
    ((a)->tv_usec op (b)->tv_usec) : ((a)->tv_sec op (b)->tv_sec))
#endif
