/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2010-2017  Frediano Ziglio
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

#ifndef _tdsguard_dWrEWKe6Aqdq6AyZsaKD0V_
#define _tdsguard_dWrEWKe6Aqdq6AyZsaKD0V_

#ifndef _freetds_config_h_
#error should include config.h before
#endif

#if HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#include "tds_sysdep_public.h"
#include <freetds/sysdep_private.h>

#define TDS_ZERO_FREE(x) do {free((x)); (x) = NULL;} while(0)
#define TDS_VECTOR_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef offsetof
#define TDS_OFFSET(type, field) offsetof(type, field)
#else
#define TDS_OFFSET(type, field) (((char*)&((type*)0)->field)-((char*)0))
#endif

#if ENABLE_EXTRA_CHECKS
# if defined(__llvm__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
# define TDS_COMPILE_CHECK(name,check) \
    _Static_assert(check,#name)
# else
# define TDS_COMPILE_CHECK(name,check) \
    extern int name[(check)?1:-1] TDS_UNUSED
# endif
# define TDS_EXTRA_CHECK(stmt) stmt
#else
# define TDS_COMPILE_CHECK(name,check) \
    extern int disabled_check_##name TDS_UNUSED
# define TDS_EXTRA_CHECK(stmt)
#endif

#if defined(__GNUC__) && __GNUC__ >= 3
# define TDS_LIKELY(x)	__builtin_expect(!!(x), 1)
# define TDS_UNLIKELY(x)	__builtin_expect(!!(x), 0)
#else
# define TDS_LIKELY(x)	(x)
# define TDS_UNLIKELY(x)	(x)
#endif

#if ENABLE_EXTRA_CHECKS && defined(__GNUC__) && __GNUC__ >= 4
#define TDS_WUR __attribute__ ((__warn_unused_result__))
#else
#define TDS_WUR
#endif

#if defined(__GNUC__) && __GNUC__ >= 2
#define TDS_UNUSED __attribute__ ((unused))
#else
#define TDS_UNUSED
#endif

#define TDS_INT2PTR(i) ((void*)(((char*)0)+((TDS_INTPTR)(i))))
#define TDS_PTR2INT(p) ((int)(((char*)(p))-((char*)0)))

#define tds_new(type, n) ((type *) malloc(sizeof(type) * (n)))
#define tds_new0(type, n) ((type *) calloc(n, sizeof(type)))

#endif
