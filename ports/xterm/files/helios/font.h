/* SPDX-License-Identifier: MIT */
#ifndef HELIOS_XTERM_FONT_H
#define HELIOS_XTERM_FONT_H
#include <X11/Xlib.h>
#include <stddef.h>
XFontStruct *helios_font_load(Display *,const char *);
int helios_font_free(Display *,XFontStruct *);
int helios_font_has_metrics(const XFontStruct *);
const XCharStruct *helios_font_char(const XFontStruct *,unsigned index);
size_t helios_font_cache_bytes(const XFontStruct *);
int helios_font_width(XFontStruct *,const char *,int);
int helios_font_width16(XFontStruct *,const XChar2b *,int);
#ifndef HELIOS_FONT_NO_REMAP
#define XLoadQueryFont helios_font_load
#define XFreeFont helios_font_free
#define XTextWidth helios_font_width
#define XTextWidth16 helios_font_width16
#endif
#endif
