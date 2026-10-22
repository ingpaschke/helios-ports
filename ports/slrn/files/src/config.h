/* -*- C -*- */

/* Note: this is for Unix only.  See slrnconf.h for other systems.  
 * Also edit slrnfeat.h for slrn features.
 */

#ifndef SLRN_CONFIG_H
#define SLRN_CONFIG_H

/* Define to 1 if translation of program messages to the user's native
 * language is requested. 
 */
#undef ENABLE_NLS
#define PACKAGE "slrn"

/* define if you want SSL support */
#undef USE_SSL

/* define if you want to use NSS compat library instead of OpenSSL */
#undef USE_NSS_COMPAT

/* define if you want to use GNU TLS library instead of OpenSSL */
#undef USE_GNUTLS

/* define if iconv is not GNU compatible */
#undef NON_GNU_ICONV

/* define if you have stdlib.h */
#define HAVE_STDLIB_H 1

/* define if you have unistd.h */
#define HAVE_UNISTD_H 1

/* define if you have fcntl.h */
#define HAVE_FCNTL_H 1
#undef HAVE_SYS_FCNTL_H

/* define if you have termios.h */
#define HAVE_TERMIOS_H 1

/* define if you have memory.h */
#undef HAVE_MEMORY_H

/* define if you have malloc.h */
#undef HAVE_MALLOC_H

/* define if you have memset */
#define HAVE_MEMSET 1

/* define if you have memcpy */
#define HAVE_MEMCPY 1
#define HAVE_MEMCMP 1
#define HAVE_MEMCHR 1

#undef HAVE_SYS_UTSNAME_H
#undef HAVE_UNAME

#undef HAVE_SIGLONGJMP

/* define if you have this. */
#define HAVE_PUTENV 1
#define HAVE_GETCWD 1

#define HAVE_VSNPRINTF 1

#define HAVE_DIRENT_H 1
#undef HAVE_SYS_NDIR_H
#undef HAVE_SYS_DIR_H
#undef HAVE_NDIR_H

#define HAVE_SYS_WAIT_H 1

/* define if you have <sys/socket.h> */
#define HAVE_SYS_SOCKET_H 1

/* define if you have <socket.h> */
#undef HAVE_SOCKET_H

/* define if you have <netinet/in.h> */
#define HAVE_NETINET_IN_H 1

/* define if you have <arpa/inet.h> */
#define HAVE_ARPA_INET_H 1

#undef HAVE_LOCALE_H
#undef HAVE_LANGINFO_H

#define HAVE_SYS_TIME_H 1

/* define if you have a "timezone" variable in time.h */
#undef HAVE_TIMEZONE

/* define if you have "tm_gmtoff" in struct tm */
#undef HAVE_TM_GMTOFF

#undef HAVE_GETDOMAINNAME

/* functions for IPv6 */
#undef HAVE_GETADDRINFO
#undef HAVE_GETNAMEINFO

/* Do we have posix signals? */
#define HAVE_SIGACTION 1
#define HAVE_SIGPROCMASK 1
#define HAVE_SIGEMPTYSET 1
#define HAVE_SIGADDSET 1

#define HAVE_ISALPHA 1
#define HAVE_ISSPACE 1
#define HAVE_ISDIGIT 1
#define HAVE_ISALNUM 1
#define HAVE_ISPUNCT 1
#undef HAVE_SETLOCALE

#undef HAVE_ICONV
#undef HAVE_UUDEVIEW_H

/* define if you have va_copy() in stdarg.h */
#define VA_COPY __builtin_va_copy

/* define if va_lists can't be copied by value */
#undef VA_COPY_AS_ARRAY

#if defined(HAVE_SIGADDSET) && defined(HAVE_SIGEMPTYSET)
# if defined(HAVE_SIGACTION) && defined(HAVE_SIGPROCMASK)
#  define SLRN_POSIX_SIGNALS
# endif
#endif

#undef mode_t
#undef pid_t
#undef uid_t
#undef pid_t

/* This is here to support snprintf.  We don't use long doubles. */
#undef HAVE_LONG_DOUBLE

#if defined(ultrix) && !defined(__GNUC__)
# ifndef NO_PROTOTYPES
#  define NO_PROTOTYPES
# endif
#endif

#ifndef REAL_UNIX_SYSTEM
# define REAL_UNIX_SYSTEM
#endif

#ifdef _AIX
# ifndef _POSIX_SOURCE
#  define _POSIX_SOURCE
# endif
# ifndef _ALL_SOURCE
#  define _ALL_SOURCE
# endif
/* This may generate warnings but the fact is that without it, xlc will 
 * INCORRECTLY inline many str* functions. */
# undef __STR__
#endif

#ifndef __unix__
# define __unix__ 1
#endif

#ifdef USE_SSL
# define SLRN_HAS_SSL_SUPPORT	1
#else
# define SLRN_HAS_SSL_SUPPORT	0
#endif

#ifdef USE_GNUTLS
# define SLTCP_HAS_GNUTLS_SUPPORT 1
#else
# define SLTCP_HAS_GNUTLS_SUPPORT 0
#endif

#define SLTCP_HAS_SSL_SUPPORT SLRN_HAS_SSL_SUPPORT

#ifdef USE_NSS_COMPAT
# define SLTCP_HAS_NSS_COMPAT 1
#else
# define SLTCP_HAS_NSS_COMPAT 0
#endif

#define SLRN_SERVER_ID_NNTP 1
#define SLRN_SERVER_ID_SPOOL 2

#define SLRN_POST_ID_NNTP 1
#define SLRN_POST_ID_INEWS 2
#define SLRN_POST_ID_PULL 3

#define SLRN_PATH_SLASH_CHAR	'/'

#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_FLOAT 4
#define SIZEOF_DOUBLE 8
#define SIZEOF_LONG_LONG 0

#undef HAVE_LONG_LONG
#undef HAVE_ATOLL
#undef HAVE_STRTOLL

#undef NNTPSERVER_NAME
#undef NNTPSERVER_FILE

#if defined(HAVE_LONG_LONG) && (SIZEOF_LONG < SIZEOF_LONG_LONG)
typedef long long NNTP_Artnum_Type;
# define NNTP_FMT_ARTNUM "%lld"
# define NNTP_FMT_ARTNUM_2 "%lld %lld"
# define NNTP_FMT_ARTNUM_3 "%lld %lld %lld"
# define NNTP_FMT_ARTRANGE "%lld-%lld"
# ifdef HAVE_ATOLL
#  define NNTP_STR_TO_ARTNUM(x) atoll(x)
# else
#  define NNTP_STR_TO_ARTNUM(x) strtoll((x),NULL,10)
# endif
# define NNTP_ARTNUM_TYPE_MAX 9223372036854775807LL
#else
typedef long NNTP_Artnum_Type;
# define NNTP_FMT_ARTNUM "%ld"
# define NNTP_FMT_ARTNUM_2 "%ld %ld"
# define NNTP_FMT_ARTNUM_3 "%ld %ld %ld"
# define NNTP_FMT_ARTRANGE "%ld-%ld"
# define NNTP_STR_TO_ARTNUM atol
# define NNTP_ARTNUM_TYPE_MAX LONG_MAX
#endif

/* The following set defines may be necessary to activate long file support */
#undef _FILE_OFFSET_BITS
#undef _LARGE_FILES
#undef _LARGEFILE_SOURCE


#endif /* SLRN_CONFIG_H */
