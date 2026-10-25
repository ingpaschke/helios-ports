/* SPDX-License-Identifier: MIT */
#ifndef XTERM_XUTIL_H
#define XTERM_XUTIL_H
#include_next <X11/Xutil.h>
int XmbTextPropertyToTextList(Display *,const XTextProperty *,char ***,int *);
int XmbTextListToTextProperty(Display *,char **,int,XICCEncodingStyle,XTextProperty *);
#endif
