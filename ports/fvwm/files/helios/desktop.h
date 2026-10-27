#ifndef HELIOS_FVWM_DESKTOP_H
#define HELIOS_FVWM_DESKTOP_H
#include <X11/Xlib.h>
int helios_desktop_open(Display *, XFontStruct *);
int helios_desktop_event(XEvent *);
/* Milliseconds until the clock needs updating; -1 after replacement. */
int helios_desktop_tick(void);
void helios_desktop_close(void);
#endif
