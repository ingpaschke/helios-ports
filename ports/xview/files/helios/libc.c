/* SPDX-License-Identifier: MIT */
#include <math.h>

char *xvh_strdup(const char *text)
{
    unsigned size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

int xvh_killpg(int group, int sig)
{
    return kill(group ? -group : 0, sig);
}

double xvh_round_pixel(double value)
{
    return value < 0 ? ceil(value - 0.5) : floor(value + 0.5);
}

/* The three command-line error messages use only %s and literal text. */
int xvh_format_strings(char *output, unsigned capacity, const char *format, ...)
{
    unsigned used = 0;
    va_list args;
    va_start(args, format);
    while (*format) {
        const char *piece;
        char literal[2];
        if (format[0] == '%' && format[1] == 's') {
            piece = va_arg(args, const char *);
            if (!piece) piece = "(null)";
            format += 2;
        } else {
            literal[0] = *format++;
            literal[1] = 0;
            piece = literal;
        }
        while (*piece) {
            if (capacity && used < capacity - 1) output[used] = *piece;
            ++used;
            ++piece;
        }
    }
    va_end(args);
    if (capacity) output[used < capacity ? used : capacity - 1] = 0;
    return used;
}

/* XView's file chooser uses the allocating BSD form of getcwd. */
#undef getcwd
char *xvh_getcwd(char *buffer, int size)
{
    char *allocated;
    int capacity, saved;
    if (buffer) return getcwd(buffer, size);
    if (size < 0) { errno = EINVAL; return NULL; }
    capacity = size ? size : 128;
    for (;;) {
        allocated = malloc(capacity);
        if (!allocated) return NULL;
        if (getcwd(allocated, capacity)) return allocated;
        saved = errno;
        free(allocated);
        if (size || saved != ERANGE) { errno = saved; return NULL; }
        if (capacity > INT_MAX / 2) { errno = ENOMEM; return NULL; }
        capacity *= 2;
    }
}
