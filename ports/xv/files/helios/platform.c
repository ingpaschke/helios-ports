/* SPDX-License-Identifier: MIT
 * Helios has no device nodes or FIFOs; its native channels replace them.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int mknod(const char *path, mode_t mode, dev_t dev)
{
    (void) path;
    (void) mode;
    (void) dev;
    errno = EFTYPE;
    return -1;
}
