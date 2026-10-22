/*
 * hcompat.c -- C library functions Frotz uses that the Helios C library
 * (C89) does not have.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "defs.h"

/* Helios has vsprintf only: format into a large buffer, then truncate.
 * Frotz formats short messages (error texts, file names). */
int hc_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	static char tmp[4096];
	int n = vsprintf(tmp, fmt, ap);

	if (size > 0) {
		size_t len = (size_t)n < size ? (size_t)n : size - 1;
		memcpy(buf, tmp, len);
		buf[len] = '\0';
	}
	return n;
}

int hc_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = hc_vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

/* POSIX dirname(): modifies path, returns "." if it has no '/'. */
char *hc_dirname(char *path)
{
	char *p;

	if (path == NULL || (p = strrchr(path, '/')) == NULL)
		return ".";
	while (p > path && p[-1] == '/')
		p--;
	if (p == path)
		return "/";
	*p = '\0';
	return path;
}
