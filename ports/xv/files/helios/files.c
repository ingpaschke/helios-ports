/* SPDX-License-Identifier: MIT
 * Filesystem operations for XV that do not interpret paths as shell text.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xv-platform.h"

struct remove_entry {
    struct remove_entry *next;
    char path[1];
};

int xv_remove_tree(char *path)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    struct remove_entry *children = NULL, *child;
    size_t length;
    int error = 0;

    if (lstat(path, &st) < 0) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    dir = opendir(path);
    if (!dir) return -1;
    length = strlen(path);
    /* Snapshot names before deletion, which can shift directory offsets. */
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) { error = errno; break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        child = malloc(sizeof(*child) + length + strlen(entry->d_name) + 1);
        if (!child) { error = ENOMEM; break; }
        strcpy(child->path, path);
        child->path[length] = '/';
        strcpy(child->path + length + 1, entry->d_name);
        child->next = children;
        children = child;
    }
    if (closedir(dir) < 0 && !error) error = errno;
    while (children) {
        child = children;
        children = child->next;
        if (!error && xv_remove_tree(child->path) < 0) error = errno;
        free(child);
    }
    if (error) { errno = error; return -1; }
    return rmdir(path);
}

int xv_uncompress(char *input, char *output)
{
    int in, out, pid, waited, status, error;

    in = open(input, O_RDONLY);
    if (in < 0) goto failed;
    /* output is the file already reserved by xv_mktemp(). */
    out = open(output, O_WRONLY | O_TRUNC);
    if (out < 0) { error = errno; close(in); errno = error; goto failed; }
    pid = vfork();
    if (pid == 0) {
        if (dup2(in, STDIN_FILENO) < 0 || dup2(out, STDOUT_FILENO) < 0)
            _exit(126);
        if (in > STDERR_FILENO) close(in);
        if (out > STDERR_FILENO) close(out);
        execlp("uncompress", "uncompress", "-c", (char *) 0);
        _exit(127);
    }
    error = errno;
    close(in);
    close(out);
    if (pid < 0) { errno = error; goto failed; }
    do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited < 0) goto failed;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    errno = EIO;
failed:
    error = errno;
    unlink(output);
    errno = error;
    return -1;
}
