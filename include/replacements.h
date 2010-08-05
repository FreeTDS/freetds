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

/* $Id: replacements.h,v 1.27 2010-08-05 08:58:36 freddy77 Exp $ */

#include <stdarg.h>
#include "tds_sysdep_public.h"

#ifndef HAVE_READPASSPHRASE
# include <replacements/readpassphrase.h>
#else
# include <readpassphrase.h>
#endif

/* these headers are needed for basename */
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_LIBGEN_H
# include <libgen.h>
#endif

#if !HAVE_POLL
#include <fakepoll.h>
#define poll(fds, nfds, timeout) fakepoll((fds), (nfds), (timeout))
#endif /* !HAVE_POLL */

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility push(hidden)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#if !HAVE_VSNPRINTF
#if  HAVE__VSNPRINTF
#undef vsnprintf
#define vsnprintf _vsnprintf
#else
int vsnprintf(char *ret, size_t max, const char *fmt, va_list ap);
#endif /*  HAVE_VSNPRINTF */
#endif /* !HAVE__VSNPRINTF */

#if !HAVE_ASPRINTF
int asprintf(char **ret, const char *fmt, ...);
#endif /* !HAVE_ASPRINTF */

#if !HAVE_VASPRINTF
int vasprintf(char **ret, const char *fmt, va_list ap);
#endif /* !HAVE_VASPRINTF */

#if !HAVE_ATOLL
tds_sysdep_int64_type atoll(const char *nptr);
#endif /* !HAVE_ATOLL */

#if !HAVE_STRTOK_R
char *strtok_r(char *str, const char *sep, char **lasts);
#endif /* !HAVE_STRTOK_R */

#if HAVE_STRLCPY
#define tds_strlcpy(d,s,l) strlcpy(d,s,l)
#else
size_t tds_strlcpy(char *dest, const char *src, size_t len);
#endif

#if HAVE_STRLCAT
#define tds_strlcat(d,s,l) strlcat(d,s,l)
#else
size_t tds_strlcat(char *dest, const char *src, size_t len);
#endif

#if HAVE_BASENAME
#define tds_basename(s) basename(s)
#else
char *tds_basename(char *path);
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
int gettimeofday (struct timeval *tv, void *tz);
int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind, offset, opterr;
#endif

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility pop
#endif

#endif
