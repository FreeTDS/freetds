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

#include "tdsstring.h"

static char  software_version[]   = "$Id: tdsstring.c,v 1.4 2002-10-24 20:35:49 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


/**
 * \defgroup dstring Dynamic string functions
 * Handle dynamic string. In this string are always valid 
 * (you don't have NULL pointer, only empty string)
 */

/* This is in a separate module because we use the pointer to discriminate allocated and not allocated */
char tds_str_empty[] = "";

/** \addtogroup dstring
 *  \@{ 
 */

/** clear all string filling with zeroes (mainly for security reason) */
void tds_dstr_zero(char **s)
{
	if (*s) memset(*s,0,strlen(*s));
}

/** free string */
void tds_dstr_free(char **s)
{
	if (*s != tds_str_empty)
		free(*s);
}

/**
 * Set string to a given buffer of characters
 * @param s      dynamic string
 * @param src    source buffer
 * @param length length of source buffer
 * @return string copied or NULL on memory error
 */
char* tds_dstr_copyn(char **s,const char* src,unsigned int length)
{
	if (*s != tds_str_empty)
		free(*s);
	*s = malloc(length+1);
	if (!*s) return NULL;
	memcpy(*s,src,length);
	(*s)[length] = 0;
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
char* tds_dstr_set(char **s,char *src)
{
	if (*s != tds_str_empty) free(*s);
	return (*s = (src));
}

/**
 * copy a string from another
 * @param s      dynamic string
 * @param src    source buffer
 * @return string copied or NULL on memory error
 */
char* tds_dstr_copy(char **s,const char* src)
{
	if (*s != tds_str_empty)
		free(*s);
	return (*s = strdup(src));
}

/** \@} */
