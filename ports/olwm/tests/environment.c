/* SPDX-License-Identifier: MIT */
/* Build on the host from the prepared OLWM tree; no X server is needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOSVENV 1
#include <X11/Xlib.h>

void *MemAlloc(unsigned int size) { return malloc(size); }
void MemFree(void *memory) { free(memory); }
#include "environ.c"

int main(void)
{
    static const struct {
        const char *input;
        int screen;
        const char *output;
    } cases[] = {
        { "192.168.178.36:19", 0, "192.168.178.36:19.0" },
        { "192.168.178.36:19.0", 1, "192.168.178.36:19.1" },
        { "host:123.4", 0, "host:123.0" },
        { ":10", 0, ":10.0" },
        { ":10.1", 2, ":10.2" }
    };
    Display display;
    Env env;
    unsigned i;
    char *initial[] = { "DISPLAY_EXTRA=keep", "DISPLAY=old:0", NULL };
    memset(&display, 0, sizeof(display));
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char **saved = environ;
        environ = initial;
        createEnv(&env, 1);
        environ = saved;
        display.display_name = (char *)cases[i].input;
        putDisplayEnv(&env, &display, cases[i].screen);
        if (strcmp(env.environ[0], "DISPLAY_EXTRA=keep") ||
            strncmp(env.environ[1], "DISPLAY=", 8) ||
            strcmp(env.environ[1] + 8, cases[i].output)) {
            fprintf(stderr, "FAIL: %s screen %d\n", cases[i].input, cases[i].screen);
            return 1;
        }
        free(env.environ[0]); free(env.environ[1]); free(env.environ);
    }
    puts("PASS: child DISPLAY addresses and exact environment-key matching");
    return 0;
}
