/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _replacements_h_
#define _replacements_h_

#include <stdarg.h>
#include "tds_sysdep_public.h"
#include <freetds/sysdep_private.h>

#include <replacements/readpassphrase.h>

/* these headers are needed for basename */
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_LIBGEN_H
# include <libgen.h>
#endif
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#if !HAVE_POLL
#include <replacements/poll.h>
#endif /* !HAVE_POLL */

#include <freetds/pushvis.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if !HAVE_ASPRINTF
#undef asprintf
int tds_asprintf(char **ret, const char *fmt, ...);
#define asprintf tds_asprintf
#endif /* !HAVE_ASPRINTF */

#if !HAVE_VASPRINTF
#undef vasprintf
int tds_vasprintf(char **ret, const char *fmt, va_list ap);
#define vasprintf tds_vasprintf
#endif /* !HAVE_VASPRINTF */

#if !HAVE_STRTOK_R
/* Some MingW define strtok_r macro thread-safe but not reentrant but we
   need both so avoid using the macro */
#undef strtok_r
char *tds_strtok_r(char *str, const char *sep, char **lasts);
#define strtok_r tds_strtok_r
#endif /* !HAVE_STRTOK_R */

#if !HAVE_STRSEP
#undef strsep
char *tds_strsep(char **stringp, const char *delim);
#define strsep tds_strsep
#endif /* !HAVE_STRSEP */

#if !HAVE_STRLCPY
size_t tds_strlcpy(char *dest, const char *src, size_t len);
#undef strlcpy
#define strlcpy(d,s,l) tds_strlcpy(d,s,l)
#endif

#if !HAVE_GETADDRINFO
typedef struct tds_addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	struct sockaddr *ai_addr;
	char *ai_canonname;
	struct tds_addrinfo *ai_next;
} tds_addrinfo;

int tds_getaddrinfo(const char *node, const char *service, const struct tds_addrinfo *hints, struct tds_addrinfo **res);
int tds_getnameinfo(const struct sockaddr *sa, size_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags);
void tds_freeaddrinfo(struct tds_addrinfo *addr);
#define addrinfo tds_addrinfo
#define getaddrinfo(n,s,h,r) tds_getaddrinfo(n,s,h,r)
#define getnameinfo(a,b,c,d,e,f,g) tds_getnameinfo(a,b,c,d,e,f,g)
#define freeaddrinfo(a) tds_freeaddrinfo(a)
#endif

#ifndef AI_FQDN
#define AI_FQDN 0
#endif

#if !HAVE_STRLCAT
size_t tds_strlcat(char *dest, const char *src, size_t len);
#undef strlcat
#define strlcat(d,s,l) tds_strlcat(d,s,l)
#endif

#if !HAVE_BASENAME
char *tds_basename(char *path);
#define basename(path) tds_basename(path)
#endif

/* 
 * Microsoft's C Runtime library is missing strcasecmp and strncasecmp. 
 * Other Win32 C runtime libraries, notably minwg, may define it. 
 * There is no symbol uniquely defined in Microsoft's header files that 
 * can be used by the preprocessor to know whether we're compiling for
 * Microsoft's library or not (or which version).  Thus there's no
 * way to automatically decide whether or not to define strcasecmp
 * in terms of stricmp.  
 * 
 * The Microsoft *compiler* defines _MSC_VER.  On the assumption that
 * anyone using their compiler is also using their library, the below 
 * tests check _MSC_VER as a proxy. 
 */
#if defined(_WIN32)
# if !defined(strcasecmp) && defined(_MSC_VER) 
#     define  strcasecmp(A, B) stricmp((A), (B))
# endif
# if !defined(strncasecmp) && defined(_MSC_VER)
#     define  strncasecmp(x,y,z) strnicmp((x),(y),(z))
# endif

#undef gettimeofday
int tds_gettimeofday (struct timeval *tv, void *tz);
#define gettimeofday tds_gettimeofday

/* Older Mingw-w64 versions don't define these flags. */
#if defined(__MINGW32__) && !defined(AI_ADDRCONFIG)
#  define AI_ADDRCONFIG 0x00000400
#endif
#if defined(__MINGW32__) && !defined(AI_V4MAPPED)
#  define AI_V4MAPPED 0x00000800
#endif

#endif

#if !HAVE_GETOPT
#undef getopt
int tds_getopt(int argc, char * const argv[], const char *optstring);
#define getopt tds_getopt

extern char *optarg;
extern int optind, offset, opterr, optreset;
#endif

#if !HAVE_SOCKETPAIR
int tds_socketpair(int domain, int type, int protocol, TDS_SYS_SOCKET sv[2]);
#define socketpair(d,t,p,s) tds_socketpair(d,t,p,s)
#endif

#if !HAVE_DAEMON
int tds_daemon(int no_chdir, int no_close);
#define daemon(d,c) tds_daemon(d,c)
#endif

char *tds_getpassarg(char *arg);
void tds_sleep_s(unsigned sec);
void tds_sleep_ms(unsigned ms);

#ifdef __cplusplus
}
#endif

#include <freetds/popvis.h>

#endif
