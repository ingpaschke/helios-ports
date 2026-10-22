/* Minimal <stdint.h> for Helios (C89 library, no stdint.h of its own).
 * Helios on the transputer: char 8, short 16, int and long 32 bits. */
#ifndef HELIOS_COMPAT_STDINT_H
#define HELIOS_COMPAT_STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define UINT8_MAX 255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#endif
