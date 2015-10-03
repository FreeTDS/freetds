/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#ifndef _tdsstring_h_
#define _tdsstring_h_

#include <freetds/pushvis.h>

/** \addtogroup dstring
 * @{ 
 */

/** Internal representation for an empty string */
extern const struct tds_dstr tds_str_empty;

/** Initializer, used to initialize string like in the following example
 * @code
 * DSTR s = DSTR_INITIALIZER;
 * @endcode
 */
#define DSTR_INITIALIZER ((struct tds_dstr*) &tds_str_empty)

/** init a string with empty */
static inline void
tds_dstr_init(DSTR * s)
{
	*(s) = DSTR_INITIALIZER;
}

/** test if string is empty */
static inline int
tds_dstr_isempty(DSTR * s)
{
	return (*s)->dstr_size == 0;
}

/**
 * Returns a buffer to edit the string.
 * Be careful to avoid buffer overflows and remember to
 * set the correct length at the end of the editing if changed.
 */
static inline char *
tds_dstr_buf(DSTR * s)
{
	return (*s)->dstr_s;
}

/** Returns a C version (NUL terminated string) of dstr */
static inline const char *
tds_dstr_cstr(DSTR * s)
{
	return (*s)->dstr_s;
}

/** Returns the length of the string in bytes */
static inline size_t
tds_dstr_len(DSTR * s)
{
	return (*s)->dstr_size;
}

/** Make a string empty */
#define tds_dstr_empty(s) \
	tds_dstr_free(s)

void tds_dstr_zero(DSTR * s);
void tds_dstr_free(DSTR * s);

DSTR* tds_dstr_dup(DSTR * s, const DSTR * src) TDS_WUR;
DSTR* tds_dstr_copy(DSTR * s, const char *src) TDS_WUR;
DSTR* tds_dstr_copyn(DSTR * s, const char *src, size_t length) TDS_WUR;
DSTR* tds_dstr_set(DSTR * s, char *src) TDS_WUR;

DSTR* tds_dstr_setlen(DSTR *s, size_t length);
DSTR* tds_dstr_alloc(DSTR *s, size_t length) TDS_WUR;

/** @} */

#include <freetds/popvis.h>

#endif /* _tdsstring_h_ */
