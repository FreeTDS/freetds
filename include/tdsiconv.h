/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2002, 2003  Brian Bruns
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

static char rcsid_tds_iconv_h[] = "$Id: tdsiconv.h,v 1.19 2003-08-01 15:24:47 freddy77 Exp $";
static void *no_unused_tds_iconv_h_warn[] = { rcsid_tds_iconv_h, no_unused_tds_iconv_h_warn };

#if HAVE_ICONV
#include <iconv.h>
#else
/* Define iconv_t for src/replacements/iconv.c. */
#undef iconv_t
typedef void* iconv_t;

/* The following EILSEQ advice is borrowed verbatim from GNU iconv.  */
/* Some systems, like SunOS 4, don't have EILSEQ. Some systems, like BSD/OS,
   have EILSEQ in a different header.  On these systems, define EILSEQ
   ourselves. */
#ifndef EILSEQ
# define EILSEQ 
#endif
#endif /* HAVE_ICONV */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef __cplusplus
extern "C"
{
#endif

#if ! HAVE_ICONV

	/* FYI, the first 4 entries look like this:
	 * 	{"ISO-8859-1",	1, 1}, -> 0
	 * 	{"US-ASCII",	1, 4}, -> 1
	 * 	{"UCS-2LE",	2, 2}, -> 2
	 * 	{"UCS-2BE",	2, 2}, -> 3
	 *
	 * These conversions are supplied by src/replacements/iconv.c for the sake of those who don't 
	 * have or otherwise need an iconv.
	 */
	enum ICONV_CD_VALUE {
		  Like_to_Like  = 0x100
		, Latin1_ASCII  = 0x01
		, ASCII_Latin1  = 0x10
		, Latin1_UCS2LE = 0x02
		, UCS2LE_Latin1 = 0x20
		, ASCII_UCS2LE  = 0x12
		, UCS2LE_ASCII  = 0x21
		/* these aren't needed 
			, Latin1_UCS2BE = 0x03
			, UCS2BE_Latin1 = 0x30
		*/
	};

	iconv_t iconv_open (const char* tocode, const char* fromcode);
	size_t iconv (iconv_t cd, const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft);
	int iconv_close (iconv_t cd);
#endif /* !HAVE_ICONV */


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
size_t tds_iconv (TDSSOCKET *tds, const TDSICONVINFO *iconv_info, TDS_ICONV_DIRECTION io, 
		  const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft);
const char * tds_canonical_charset_name(const char *charset_name);
const char * tds_sybase_charset_name(const char *charset_name);

#ifdef __cplusplus
}
#endif

#endif /* _tds_iconv_h_ */
