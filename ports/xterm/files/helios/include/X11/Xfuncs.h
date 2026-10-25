/* SPDX-License-Identifier: MIT */
#include <string.h>
#ifndef bzero
#define bzero(p,n) memset(p,0,n)
#define bcopy(s,d,n) memmove(d,s,n)
#define bcmp(a,b,n) memcmp(a,b,n)
#endif
