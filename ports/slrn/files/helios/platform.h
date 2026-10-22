#ifndef HELIOS_SLRN_PLATFORM_H
#define HELIOS_SLRN_PLATFORM_H
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <helios/unix/api.h>
#include <helios/unix/netdb.h>
#include <helios/unix/spawn.h>
#include <helios/unix/tls_fd.h>
#define ICANON ICANNON
#define ENOSYS EOPNOTSUPP
#define ENOTSUP EOPNOTSUPP
#define PATH_MAX 1024
#define MAXPATHLEN 1024
typedef int ssize_t;
int helios_system(const char *);
FILE *helios_popen(const char *,const char *);
int helios_pclose(FILE *);
int helios_no_fork(void);
#undef fork
#define fork helios_no_fork
int vsnprintf(char *,size_t,const char *,va_list);
#define system helios_system
#define popen helios_popen
#define pclose helios_pclose
#endif
