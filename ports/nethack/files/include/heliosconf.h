/* Helios 1.3, native X11R4 window port. */
#ifndef HELIOSCONF_H
#define HELIOSCONF_H
#define SYSV
#define POSIX_TYPES
#define TEXTCOLOR
#define NO_FILE_LINKS
#define LOCKDIR "."
#define HLOCK "perm"
#define FCMASK 0660
#define RANDOM
#define Rand() random()
#include "system.h"
#define index strchr
#define rindex strrchr
#define tgetch getchar
/* Keep the bundled BSD generator separate from host libc when building tools. */
#define random nh_random
#define srandom nh_srandom
#define initstate nh_initstate
#define setstate nh_setstate
extern long random(void);
extern void srandom(unsigned int);
#endif
