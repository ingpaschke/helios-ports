/* SPDX-License-Identifier: MIT */
#ifndef XTERM_WCTYPE_H
#define XTERM_WCTYPE_H
typedef unsigned int wint_t;
static int iswcntrl(wint_t c) { return c<32 || (c>=127 && c<160); }
#endif
