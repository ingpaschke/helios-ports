/* NetHack may be freely redistributed. See license for details.
 * Helios has no separate effective uid. Keyboard translation is supplied
 * by the shared X11 library, including modifier and mapping changes. */
#include <sys/types.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
typedef char nh_xchar2b_size[(sizeof(XChar2b) == 2) ? 1 : -1];
int seteuid(uid_t uid) { (void)uid; return 0; }
