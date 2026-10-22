/* Exercise the real lock code against DOS-style open-file deletion rules. */
#include "hack.h"
#include <assert.h>
#include <errno.h>

int done_hup = 1;
struct window_procs windowprocs;
static int lock_exists, lock_open;

void raw_printf(const char *format, ...) { (void) format; }

int __wrap_open(const char *path, int flags, ...)
{
    assert(strcmp(path, "./perm.lck") == 0);
    assert(flags == (O_RDWR | O_CREAT | O_EXCL));
    if (lock_exists) {
        errno = EEXIST;
        return -1;
    }
    lock_exists = lock_open = 1;
    return 42;
}

int __wrap_close(int fd)
{
    assert(fd == 42 && lock_open);
    lock_open = 0;
    return 0;
}

int __wrap_unlink(const char *path)
{
    assert(strcmp(path, "./perm.lck") == 0);
    assert(lock_exists);
    if (lock_open) {
        errno = EACCES;
        return -1;
    }
    lock_exists = 0;
    return 0;
}

int main(void)
{
    /* An existing lock must remain intact when acquisition fails. */
    lock_exists = 1;
    assert(!lock_file(HLOCK, 0));
    assert(lock_exists && !lock_open);
    lock_exists = 0;

    assert(lock_file(HLOCK, 0));
    assert(lock_exists && lock_open);
    unlock_file(HLOCK);
    assert(!lock_exists && !lock_open);

    /* A second launch must be able to acquire the released lock. */
    assert(lock_file(HLOCK, 0));
    unlock_file(HLOCK);
    assert(!lock_exists && !lock_open);
    puts("PASS: DOS lock release, reacquisition and existing-lock protection");
    return 0;
}
