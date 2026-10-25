/* SPDX-License-Identifier: MIT
 * Helios declarations that xv expects from its host C library.
 */
#ifndef XV_HELIOS_PLATFORM_H
#define XV_HELIOS_PLATFORM_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private XV adapter: reserves an empty file; callers must unlink it. */
char *xv_mktemp(char *template);
#define mktemp xv_mktemp

int xv_remove_tree(char *path);
int xv_uncompress(char *input, char *output);

/* Helios has no device or FIFO nodes; refuse copies of special files. */
int mknod(const char *path, mode_t mode, dev_t dev);

/* Helios declares select() in <sys/socket.h>, which xv does not include. */
#ifdef helios
extern int select(int nfds, int *readfds, int *writefds, int *exceptfds,
                  struct timeval *timeout);
#endif

#ifdef __cplusplus
}
#endif

#endif
