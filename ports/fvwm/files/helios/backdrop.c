/* Standalone entry point; FVWM can host the same renderer in its event loop. */
#include "platform.h"
#include "desktop.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    Display *display;
    const char *display_name = NULL;
    int use_cache = 0;
    int i;
    XEvent event;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-cache")) {
            use_cache = 1;
        } else if (!strcmp(argv[i], "-display") && i + 1 < argc) {
            display_name = argv[++i];
        } else {
            fprintf(stderr, "Usage: helios-backdrop [-cache] [-display display]\n");
            return 1;
        }
    }
    display = XOpenDisplay(display_name);
    if (!display) {
        return 1;
    }
    if (!helios_desktop_open(display, NULL, use_cache)) {
        XCloseDisplay(display);
        return 1;
    }
    for (;;) {
        fd_set ready;
        struct timeval timeout;
        int delay;

        while (XPending(display)) {
            XNextEvent(display, &event);
            helios_desktop_event(&event);
        }
        delay = helios_desktop_tick();
        if (delay < 0) {
            break;
        }
        FD_ZERO(&ready);
        FD_SET(ConnectionNumber(display), &ready);
        timeout.tv_sec = delay / 1000;
        timeout.tv_usec = (delay % 1000) * 1000;
        if (ktm_select(ConnectionNumber(display) + 1, &ready, NULL, NULL,
                       &timeout) < 0 && errno != EINTR) {
            break;
        }
    }
    helios_desktop_close();
    XCloseDisplay(display);
    return 0;
}
