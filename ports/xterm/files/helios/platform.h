/* SPDX-License-Identifier: MIT */
/* Interfaces missing from the X11R4 and native Helios headers. */
#ifndef XTERM_HELIOS_PLATFORM_H
#define XTERM_HELIOS_PLATFORM_H
#include <stddef.h>
#include <sys/types.h>
#define Object HeliosObject
#include <termios.h>
#undef Object
#include <sys/resource.h>
#define ICANON ICANNON
typedef unsigned char cc_t;
#define _XtBoolean Boolean
#define XtRGravity "Gravity"
#define X_GETTIMEOFDAY(p) gettimeofday(p,NULL)
#define XrmPermStringToQuark XrmStringToQuark
#define sessionShellWidgetClass applicationShellWidgetClass
typedef void *IceConn;
typedef void *XIC;
typedef enum {XStringStyle,XCompoundTextStyle,XTextStyle,XStdICCTextStyle,XUTF8StringStyle} XICCEncodingStyle;
#define XConverterNotFound -3
#define XNoMemory -1
#define XLocaleNotSupported -2
#define XBufferOverflow -1
#define XLookupNone 1
#define XLookupChars 2
#define XLookupKeySym 3
#define XLookupBoth 4
#define sigjmp_buf jmp_buf
#define sigsetjmp(b,s) setjmp(b)
#define siglongjmp(b,n) longjmp(b,n)
typedef int XtGravity;
typedef char *XPointer;
#ifndef XtOffsetOf
#define XtOffsetOf(s,m) offsetof(s,m)
#endif
struct helios_winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
int helios_get_winsize(int,struct helios_winsize *);
int helios_set_winsize(int,const struct helios_winsize *);
#define TTYSIZE_STRUCT struct helios_winsize
#define USE_STRUCT_WINSIZE 1
#define GET_TTYSIZE(fd,data) helios_get_winsize(fd,&data)
#define SET_TTYSIZE(fd,data) helios_set_winsize(fd,&data)
#define TTYSIZE_COLS(data) data.ws_col
#define TTYSIZE_ROWS(data) data.ws_row
#include "helios-pty.h"
#include "font.h"
#endif
