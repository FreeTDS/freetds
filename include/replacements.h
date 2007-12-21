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

/* $Id: replacements.h,v 1.18 2007-12-21 09:06:53 freddy77 Exp $ */

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

#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC visibility push(hidden)
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

#if defined(HAVE__VSNPRINTF) && !defined(HAVE_VSNPRINTF)
#undef vsnprintf
#define vsnprintf _vsnprintf
#elif !HAVE_VSNPRINTF
int vsnprintf(char *ret, size_t max, const char *fmt, va_list ap);
#endif /* !HAVE_VSNPRINTF */

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

#if defined(WIN32)
int gettimeofday (struct timeval *tv, void *tz);
#endif

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#pragma GCC visibility pop
#endif

#endif
