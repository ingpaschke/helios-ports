/* Standalone entry point; FVWM can host the same renderer in its event loop. */
#include "platform.h"
#include "desktop.h"
int main(void)
{
    Display *d=XOpenDisplay(0);
    XEvent event;
    if (!d) return 1;
    helios_desktop_open(d,0);
    for (;;) {
        fd_set ready;
        struct timeval timeout;
        int delay;
        while (XPending(d)) {
            XNextEvent(d,&event);helios_desktop_event(&event);
        }
        delay=helios_desktop_tick();
        if (delay<0) break;
        FD_ZERO(&ready);FD_SET(ConnectionNumber(d),&ready);
        timeout.tv_sec=delay/1000;timeout.tv_usec=(delay%1000)*1000;
        if (ktm_select(ConnectionNumber(d)+1,&ready,0,0,&timeout)<0 && errno!=EINTR)
            break;
    }
    helios_desktop_close();XCloseDisplay(d);return 0;
}
