/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NP_PLATFORM_H
#define NP_PLATFORM_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __HELIOS
#include <nanoprintf.h>
#define snprintf npf_snprintf
#define vsnprintf npf_vsnprintf
#define strtoull np_strtoull
unsigned long long np_strtoull (const char *, char **, int);
int np_usleep (unsigned int microseconds);
#define usleep np_usleep
#endif

#endif
