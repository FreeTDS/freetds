/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008  Ziglio Frediano
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <freetds/odbc.h>

#include <freetds/iconv.h>
#include <freetds/encodings.h>

/* Compile-time check that sizes are defined correctly
 * (otherwise the #if in odbc.h can malfunction)
 */
typedef char sizecheck_sqlwchar[(SIZEOF_SQLWCHAR == sizeof(SQLWCHAR)) ? 1 : -1];
typedef char sizecheck_wchar_t [(SIZEOF_WCHAR_T  == sizeof(wchar_t) ) ? 1 : -1];

#ifndef sqlwcslen
size_t sqlwcslen(const SQLWCHAR * s)
{
	const SQLWCHAR *p = s;

	while (*p)
		++p;
	return p - s;
}
#endif

#ifdef ENABLE_ODBC_WIDE
#if SIZEOF_SQLWCHAR != SIZEOF_WCHAR_T
/**
 * Convert a SQLWCHAR string into a wchar_t
 * Used only for debugging purpose
 * \param str string to convert
 * \param bufs linked list of buffer
 * \return string converted
 */
const wchar_t *sqlwstr(const SQLWCHAR *str, SQLWSTRBUF **bufs)
{
	wchar_t *dst, *dst_end;
	const SQLWCHAR *src = str;
	SQLWSTRBUF *buf;

	if (!str)
		return NULL;

	/* allocate buffer for string, we do not care for memory errors */
	buf = tds_new0(SQLWSTRBUF, 1);
	if (!buf)
		return NULL;
	buf->next = *bufs;
	*bufs = buf;

	dst = buf->buf;
	dst_end = dst + (TDS_VECTOR_SIZE(buf->buf) - 1);

	for (; *src && dst < dst_end; *dst++ = *src++)
		continue;
	*dst = L'\0';

	return buf->buf;
}

void sqlwstr_free(SQLWSTRBUF *bufs)
{
	while (bufs) {
		SQLWSTRBUF *buf = bufs;
		bufs = buf->next;
		free(buf);
	}
}
#endif
#endif

#if SIZEOF_SQLWCHAR == 2
# if WORDS_BIGENDIAN
#  define ODBC_WIDE_CANONIC TDS_CHARSET_UCS_2BE
#  define ODBC_WIDE_CANONIC_UTF TDS_CHARSET_UTF_16BE
# else
#  define ODBC_WIDE_CANONIC TDS_CHARSET_UCS_2LE
#  define ODBC_WIDE_CANONIC_UTF TDS_CHARSET_UTF_16LE
# endif
#elif SIZEOF_SQLWCHAR == 4
# if WORDS_BIGENDIAN
#  define ODBC_WIDE_CANONIC TDS_CHARSET_UCS_4BE
# else
#  define ODBC_WIDE_CANONIC TDS_CHARSET_UCS_4LE
# endif
#else
#error SIZEOF_SQLWCHAR not supported !!
#endif

int odbc_get_wide_canonic(TDSCONNECTION *conn)
{
#if SIZEOF_SQLWCHAR == 2
	if (conn->char_convs[client2ucs2]->to.charset.canonic == TDS_CHARSET_UTF_16LE)
		return ODBC_WIDE_CANONIC_UTF;
#endif
	return ODBC_WIDE_CANONIC;
}
