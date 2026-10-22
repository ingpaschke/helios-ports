/* SPDX-License-Identifier: MIT */
#ifndef XVH_TERMIOS_NAMESPACE_H
#define XVH_TERMIOS_NAMESPACE_H
/* Helios termios includes nucleus headers. Keep their names out of XView. */
#ifndef XVH_NATIVE_TERMINAL
#define byte XvhNativeByte
#define string XvhNativeString
#define Stream XvhNativeStream
#endif
#include_next <termios.h>
#ifndef XVH_NATIVE_TERMINAL
#undef byte
#undef string
#undef Stream
#endif
#ifndef ICANON
#define ICANON ICANNON
#endif
#endif
