/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2002, 2003, 2004  Brian Bruns
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

#if HAVE_ICONV
#include <iconv.h>
#else
/* Define iconv_t for src/replacements/iconv.c. */
#undef iconv_t
typedef void *iconv_t;
#endif /* HAVE_ICONV */

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_WCHAR_H
#include <wchar.h>
#endif

/* The following EILSEQ advice is borrowed verbatim from GNU iconv.  */
/* Some systems, like SunOS 4, don't have EILSEQ. Some systems, like BSD/OS,
   have EILSEQ in a different header.  On these systems, define EILSEQ
   ourselves. */
#ifndef EILSEQ
# define EILSEQ ENOENT
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <freetds/pushvis.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if ! HAVE_ICONV
iconv_t tds_sys_iconv_open(const char *tocode, const char *fromcode);
size_t tds_sys_iconv(iconv_t cd, const char **inbuf, size_t * inbytesleft, char **outbuf, size_t * outbytesleft);
int tds_sys_iconv_close(iconv_t cd);
#else
#define tds_sys_iconv_open iconv_open
#define tds_sys_iconv iconv
#define tds_sys_iconv_close iconv_close
#endif /* !HAVE_ICONV */


typedef enum
{ to_server, to_client } TDS_ICONV_DIRECTION;

typedef struct _character_set_alias
{
	const char *alias;
	int canonic;
} CHARACTER_SET_ALIAS;

typedef struct tds_errno_message_flags {
	unsigned int e2big:1;
	unsigned int eilseq:1;
	unsigned int einval:1;
} TDS_ERRNO_MESSAGE_FLAGS;

typedef struct tdsiconvdir
{
	TDS_ENCODING charset;

	iconv_t cd;
} TDSICONVDIR;

struct tdsiconvinfo
{
	struct tdsiconvdir to, from;

#define TDS_ENCODING_MEMCPY   1
	unsigned int flags;

	/* 
	 * Suppress error messages that would otherwise be emitted by tds_iconv().
	 * Functions that process large buffers ask tds_iconv to convert it in "chunks".
	 * We don't want to emit spurious EILSEQ errors or multiple errors for one 
	 * buffer.  tds_iconv() checks this structure before emiting a message, and 
	 * adds to it whenever it emits one.  Callers that handle a particular situation themselves
	 * can prepopulate it.  
	 */ 
	TDS_ERRNO_MESSAGE_FLAGS suppress;

};

/* We use ICONV_CONST for tds_iconv(), even if we don't have iconv() */
#ifndef ICONV_CONST
# define ICONV_CONST const
#endif

size_t tds_iconv(TDSSOCKET * tds, TDSICONV * char_conv, TDS_ICONV_DIRECTION io,
		 const char **inbuf, size_t * inbytesleft, char **outbuf, size_t * outbytesleft);
const char *tds_canonical_charset_name(const char *charset_name);
TDSICONV *tds_iconv_get(TDSCONNECTION * conn, const char *client_charset, const char *server_charset);
TDSICONV *tds_iconv_get_info(TDSCONNECTION * conn, int canonic_client, int canonic_server);

#ifdef __cplusplus
}
#endif

#include <freetds/popvis.h>

#endif /* _tds_iconv_h_ */
