/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "tds.h"
#include "tdsstring.h"

static char software_version[] = "$Id: tdsstring.c,v 1.8 2003-04-21 09:05:59 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


/**
 * \ingroup libtds
 * \defgroup dstring Dynamic string functions
 * Handle dynamic string. In this string are always valid 
 * (you don't have NULL pointer, only empty string)
 */

/* This is in a separate module because we use the pointer to discriminate allocated and not allocated */
char tds_str_empty[] = "";

/** \addtogroup dstring
 *  \@{ 
 */

struct DSTR
{
	char c;
};

/** clear all string filling with zeroes (mainly for security reason) */
void
tds_dstr_zero(DSTR * s)
{
	if (*(char **) s)
		memset(*(char **) s, 0, strlen(*(char **) s));
}

/** free string */
void
tds_dstr_free(DSTR * s)
{
	if (*(char **) s != tds_str_empty)
		free(*(char **) s);
}

/**
 * Set string to a given buffer of characters
 * @param s      dynamic string
 * @param src    source buffer
 * @param length length of source buffer
 * @return string copied or NULL on memory error
 */
DSTR
tds_dstr_copyn(DSTR * s, const char *src, unsigned int length)
{
	if (*(char **) s != tds_str_empty)
		free(*(char **) s);
	*(char **) s = (char *) malloc(length + 1);
	if (!*(char **) s)
		return NULL;
	memcpy(*(char **) s, src, length);
	(*(char **) s)[length] = 0;
	return *s;
}

/**
 * set a string from another buffer. 
 * The string will use the supplied buffer (it not copy the string),
 * so it should be a pointer returned by malloc.
 * @param s      dynamic string
 * @param src    source buffer
 * @return string copied or NULL on memory error
 */
DSTR
tds_dstr_set(DSTR * s, char *src)
{
	if (*(char **) s != tds_str_empty)
		free(*(char **) s);
	*(char **) s = src;
	return *s;
}

/**
 * copy a string from another
 * @param s      dynamic string
 * @param src    source buffer
 * @return string copied or NULL on memory error
 */
DSTR
tds_dstr_copy(DSTR * s, const char *src)
{
	if (*(char **) s != tds_str_empty)
		free(*(char **) s);
	*(char **) s = strdup(src);
	return *s;
}

#if ENABLE_EXTRA_CHECKS
void
tds_dstr_init(DSTR * s)
{
	*(char **) s = tds_str_empty;
}

int
tds_dstr_isempty(DSTR * s)
{
	return **(char **) s == 0;
}

char *
tds_dstr_cstr(DSTR * s)
{
	return *(char **) s;
}

size_t
tds_dstr_len(DSTR * s)
{
	return strlen(*(char **) s);
}
#endif

/** \@} */
