#ifndef HELIOS_LINKS_PLATFORM_H
#define HELIOS_LINKS_PLATFORM_H
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <helios/unix/api.h>
#include <helios/unix/channel.h>
#include <helios/unix/netdb.h>
#include <helios/unix/spawn.h>
#include <helios/unix/resolve.h>
#include <helios/unix/tls_fd.h>
#define ICANON ICANNON
#define ENOSYS EOPNOTSUPP
#define ENOTSUP EOPNOTSUPP
#define PATH_MAX 1024
#define MAXPATHLEN 1024
#define INADDR_BROADCAST 0xffffffffUL
int links_no_fork(void);
int links_terminal_read(int, unsigned char *, int);
#endif
