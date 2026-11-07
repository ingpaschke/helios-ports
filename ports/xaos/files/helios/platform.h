/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HELIOS_XAOS_PLATFORM_H
#define HELIOS_XAOS_PLATFORM_H
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>
char *strdup(const char *);
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef isnan
#define isnan(x) __builtin_isnan(x)
#endif
int hx_usleep(unsigned int);
#define usleep hx_usleep

#endif
