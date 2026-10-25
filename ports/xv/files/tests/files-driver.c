/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

extern char *xv_mktemp(char *name);
extern int xv_remove_tree(char *path);
extern int xv_uncompress(char *input, char *output);
#define mktemp xv_mktemp
typedef unsigned char byte;
#define ISTR_INFO 0
#define ISTR_WARNING 1
#define DEBUG 0
#define BF_DIR 1
#define OWRT_ASK 0
#define OWRT_NOASK 1
#define OWRT_CANCEL 2
#define ERRSTR(x) strerror(x)
#define ISPIPE(c) ((c) == '|')
static char *tmpdir;
static char path[1], outFName[256];
static int dopipe;
static int overwrite = OWRT_NOASK;

long TaskData(long action, void *result)
{
    char *port = getenv("XV_TEST_PORT");
    *(long *) result = port ? strtol(port, NULL, 10) : 42;
    return 0;
}
long MachineName(char *name)
{
    char *machine = getenv("XV_TEST_MACHINE");
    strcpy(name, machine ? machine : "/test/cpu");
    return 0;
}
time_t xv_test_time(time_t *result)
{
    if (result) *result = 1700000000;
    return 1700000000;
}
static void SetISTR(int kind, const char *format, ...) {}
static void Warning(void) {}
static char *BaseName(char *name) { return name; }
static void ErrPopUp(char *text, char *button) {}
/* An unexpected overwrite prompt must cause the test to fail. */
static int PopUp(char *text, char **buttons, int count) { return 1; }
static FILE *xv_fopen(char *name, char *mode) { return fopen(name, mode); }
static int wrheader(FILE *fp, int nx, int ny, char *comment)
{
    return getenv("XV_TEST_HEADER_FAIL") ? 1 : 0;
}
static int stat2bf(unsigned int mode) { return S_ISDIR(mode) ? BF_DIR : 0; }
/* Simulate successful directory copying to reach moveFile's EXDEV cleanup. */
static int copyFile(char *src, char *dst) { return mkdir(dst, 0700) < 0; }
static int test_rename(const char *src, const char *dst)
{
    if (getenv("XV_TEST_EXDEV")) { errno = EXDEV; return -1; }
    return rename(src, dst);
}
#define rename test_rename

/* Generated from the patched upstream source by test_files.py. */
#include "upstream.c"

int main(int argc, char **argv)
{
    char name[512];
    FILE *fp;
    byte image[2] = { 10, 20 };
    int result;

    if (!strcmp(argv[1], "name")) {
        strcpy(name, argv[2]);
        puts(xv_mktemp(name));
        return name[0] ? 0 : 1;
    }
    if (!strcmp(argv[1], "remove")) return xv_remove_tree(argv[2]) < 0;
    if (!strcmp(argv[1], "move")) return moveFile(argv[2], argv[3]);
    if (!strcmp(argv[1], "uncompress")) {
        tmpdir = argv[3];
        result = UncompressFile(argv[2], name);
        puts(name);
        return result ? 0 : 1;
    }
    if (!strcmp(argv[1], "fits")) {
        tmpdir = argv[2];
        splitfits(image, "", 1, 1, atoi(argv[3]), name);
        puts(name);
        return 0;
    }
    if (!strcmp(argv[1], "cleanup")) {
        KillPageFiles(argv[2], atoi(argv[3]));
        return 0;
    }
    if (!strcmp(argv[1], "openpipe")) {
        tmpdir = argv[2];
        fp = OpenOutFile("|cat");
        if (!fp) return 1;
        fclose(fp);
        puts(outFName);
        return 0;
    }
    return 2;
}
