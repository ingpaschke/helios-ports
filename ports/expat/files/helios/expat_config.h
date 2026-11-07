/* SPDX-License-Identifier: MIT */
#ifndef EXPAT_CONFIG_H
#define EXPAT_CONFIG_H
#define BYTEORDER 1234
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_INTTYPES_H 1
#define XML_CONTEXT_BYTES 1024
#define XML_GE 1
#define XML_NS 1
/* Helios has no kernel entropy source. Nonpareil reads installed definitions. */
#define XML_POOR_ENTROPY 1
#endif
