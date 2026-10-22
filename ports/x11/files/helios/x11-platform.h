/* ABI and OS declarations for X11R4 on current 32-bit Helios. */
#ifndef KTM_X11_PLATFORM_H
#define KTM_X11_PLATFORM_H
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#define FNDELAY O_NONBLOCK
#define NOFILE 64
#define getdtablesize() 64
#define ERRORDB "/helios/lib/X11/XErrorDB"
#define KEYSYMDB "/helios/lib/X11/XKeysymDB"
#define XFILESEARCHPATHDEFAULT "/helios/lib/X11/%L/%T/%N%S:/helios/lib/X11/%T/%N%S"
#define BITMAPDIR "/helios/include/x11r4/X11/bitmaps"
#endif
