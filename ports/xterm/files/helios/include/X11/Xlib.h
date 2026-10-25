/* SPDX-License-Identifier: MIT */
#ifndef XTERM_XLIB_H
#define XTERM_XLIB_H
#include_next <X11/Xlib.h>
Status helios_parse_color(Display *,Colormap,const char *,XColor *);
#define XParseColor helios_parse_color
#endif
