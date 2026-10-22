/* SPDX-License-Identifier: MIT */
/* XView's system-call boundary, using native Helios readiness subscriptions. */
#undef read
#undef fcntl
#undef select
#include <sys/socket.h>
#include <limits.h>

static struct timeval real_interval;
static long long real_deadline;

static long long microseconds(const struct timeval *tv)
{
    return (long long)tv->tv_sec * 1000000 + tv->tv_usec;
}

static long long now_us(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return microseconds(&now);
}

int xvh_getitimer(int which, struct itimerval *value)
{
    long long remaining;
    if (which != ITIMER_REAL || !value) {
        errno = EINVAL;
        return -1;
    }
    value->it_interval = real_interval;
    remaining = real_deadline ? real_deadline - now_us() : 0;
    if (remaining < 0) remaining = 0;
    value->it_value.tv_sec = remaining / 1000000;
    value->it_value.tv_usec = remaining % 1000000;
    return 0;
}

int xvh_setitimer(int which, const struct itimerval *value,
                  struct itimerval *previous)
{
    if (which != ITIMER_REAL || !value ||
        (long)value->it_value.tv_sec < 0 || (long)value->it_interval.tv_sec < 0 ||
        value->it_value.tv_usec < 0 || value->it_value.tv_usec >= 1000000 ||
        value->it_interval.tv_usec < 0 || value->it_interval.tv_usec >= 1000000) {
        errno = EINVAL;
        return -1;
    }
    if (previous && xvh_getitimer(which, previous) < 0) return -1;
    real_interval = value->it_interval;
    real_deadline = timerisset(&value->it_value) ?
        now_us() + microseconds(&value->it_value) : 0;
    return 0;
}

static int dispatch_timer(void)
{
    long long now, interval;
    if (!real_deadline || (now = now_us()) < real_deadline) return 0;
    interval = microseconds(&real_interval);
    /* Coalesce missed ticks rather than flooding the application. */
    if (interval)
        real_deadline += ((now - real_deadline) / interval + 1) * interval;
    else
        real_deadline = 0;
    raise(SIGALRM);
    errno = EINTR;
    return 1;
}

int notify_select(int width, fd_set *rd, fd_set *wr, fd_set *ex,
                  const struct timeval *timeout)
{
    struct timeval bounded;
    struct timeval *wait = NULL;
    long long wait_us = timeout ? microseconds(timeout) : -1;
    int result;
    if (width < 0 || width > FD_SETSIZE ||
        (timeout && ((long)timeout->tv_sec < 0 ||
         timeout->tv_usec < 0 || timeout->tv_usec >= 1000000))) {
        errno = EINVAL;
        return -1;
    }
    if (dispatch_timer()) return -1;
    if (real_deadline) {
        long long remaining = real_deadline - now_us();
        if (remaining < 0) remaining = 0;
        if (wait_us < 0 || remaining < wait_us) wait_us = remaining;
    }
    if (wait_us >= 0) {
        bounded.tv_sec = wait_us / 1000000;
        bounded.tv_usec = wait_us % 1000000;
        wait = &bounded;
    }
    /* Native select also understands the selectable tty master, unlike the
     * restricted hu_poll API. FIFO wakeups remain native message channels. */
    result = select(width, (int *)rd, (int *)wr, (int *)ex, wait);
    if (result < 0) return -1;
    if (dispatch_timer()) return -1;
    return result;
}

extern int xvh_terminal_read(int, char *, int);

int notify_read(int fd, char *buffer, int size)
{
    if (size < 0) { errno = EINVAL; return -1; }
    return xvh_terminal_read(fd, buffer, size);
}

int notify_fcntl(int fd, int command, int argument)
{
    return fcntl(fd, command, argument);
}
