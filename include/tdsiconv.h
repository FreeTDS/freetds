/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2002  Brian Bruns
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

#ifndef _tds_iconv_h_
#define _tds_iconv_h_

static char rcsid_tds_iconv_h[] = "$Id: tdsiconv.h,v 1.15 2003-06-08 09:11:56 freddy77 Exp $";
static void *no_unused_tds_iconv_h_warn[] = { rcsid_tds_iconv_h, no_unused_tds_iconv_h_warn };

#if HAVE_ICONV
#include <iconv.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef __cplusplus
extern "C"
{
#endif


typedef enum { to_server, to_client } TDS_ICONV_DIRECTION;

typedef struct _character_set_alias
{
	const char *alias;
	int canonic;
} CHARACTER_SET_ALIAS;

/* we use ICONV_CONST for tds_iconv(), even if we don't have iconv() */
#ifndef ICONV_CONST
# define ICONV_CONST const
#endif

size_t tds_iconv_fread(iconv_t cd, FILE * stream, size_t field_len, size_t term_len, char *outbuf, size_t *outbytesleft);
size_t tds_iconv (TDS_ICONV_DIRECTION io, const TDSICONVINFO *info, const char *input, size_t * input_size, char *out_string, size_t maxlen);
const char * tds_canonical_charset_name(const char *charset_name);
const char * tds_sybase_charset_name(const char *charset_name);

#ifdef __cplusplus
}
#endif

#endif /* _tds_iconv_h_ */
