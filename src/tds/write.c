/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
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

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tds.h"
#include "tdsiconv.h"
#include "tdsbytes.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: write.c,v 1.77 2009-01-16 20:27:59 jklowden Exp $");

/**
 * \addtogroup network
 * @{ 
 */

/*
 * CRE 01262002 making buf a void * means we can put any type without casting
 *		much like read() and memcpy()
 */
int
tds_put_n(TDSSOCKET * tds, const void *buf, size_t n)
{
	size_t left;
	const unsigned char *bufp = (const unsigned char *) buf;

	assert(n >= 0);

	for (; n;) {
		left = tds->env.block_size - tds->out_pos;
		if (left <= 0) {
			tds_write_packet(tds, 0x0);
			continue;
		}
		if (left > n)
			left = n;
		if (bufp) {
			memcpy(tds->out_buf + tds->out_pos, bufp, left);
			bufp += left;
		} else {
			memset(tds->out_buf + tds->out_pos, 0, left);
		}
		tds->out_pos += (unsigned int)left;
		n -= left;
	}
	return 0;
}

/**
 * Output a string to wire
 * automatic translate string to unicode if needed
 * \return bytes written to wire
 * \param tds state information for the socket and the TDS protocol
 * \param s   string to write
 * \param len length of string in characters, or -1 for null terminated
 */
int
tds_put_string(TDSSOCKET * tds, const char *s, int len)
{
	TDS_ENCODING *client, *server;
	char outbuf[256], *poutbuf;
	size_t inbytesleft, outbytesleft, bytes_out = 0;

	client = &tds->char_convs[client2ucs2]->client_charset;
	server = &tds->char_convs[client2ucs2]->server_charset;

	if (len < 0) {
		if (client->min_bytes_per_char == 1) {	/* ascii or UTF-8 */
			len = (int)strlen(s);
		} else if (client->min_bytes_per_char == 2 && client->max_bytes_per_char == 2) {	/* UCS-2 or variant */
			const char *p = s;

			while (p[0] || p[1])
				p += 2;
			len = (int)(p - s);

		} else {
			assert(client->min_bytes_per_char < 3);	/* FIXME */
		}
	}

	assert(len >= 0);

	/* valid test only if client and server share a character set. TODO conversions for Sybase */
	if (!IS_TDS7_PLUS(tds))	{
		tds_put_n(tds, s, len);
		return len;
	}

	memset(&tds->char_convs[client2ucs2]->suppress, 0, sizeof(tds->char_convs[client2ucs2]->suppress));
	tds->char_convs[client2ucs2]->suppress.e2big = 1;
	inbytesleft = len;
	while (inbytesleft) {
		tdsdump_log(TDS_DBG_NETWORK, "tds_put_string converting %d bytes of \"%.*s\"\n", (int) inbytesleft, (int) inbytesleft, s);
		outbytesleft = sizeof(outbuf);
		poutbuf = outbuf;
		
		if ((size_t)-1 == tds_iconv(tds, tds->char_convs[client2ucs2], to_server, &s, &inbytesleft, &poutbuf, &outbytesleft)) {
		
			if (errno == EINVAL) {
				tdsdump_log(TDS_DBG_NETWORK, "tds_put_string: tds_iconv() encountered partial sequence. "
							     "%d bytes remain.\n", (int) inbytesleft);
				/* TODO return some sort or error ?? */
				break;
			} else if (errno != E2BIG) {
				/* It's not an incomplete multibyte sequence, or it IS, but we're not anticipating one. */
				tdsdump_log(TDS_DBG_NETWORK, "Error: tds_put_string: "
							     "Gave up converting %d bytes due to error %d.\n", 
							     (int) inbytesleft, errno);
				tdsdump_dump_buf(TDS_DBG_NETWORK, "Troublesome bytes", s, (int) inbytesleft);
			}

			if (poutbuf == outbuf) {	/* tds_iconv did not convert anything, avoid infinite loop */
				tdsdump_log(TDS_DBG_NETWORK, "Error: tds_put_string: No conversion possible, giving up.\n");
				break;
			}
		}
		
		bytes_out += poutbuf - outbuf;
		tds_put_n(tds, outbuf, poutbuf - outbuf);
	}
	tdsdump_log(TDS_DBG_NETWORK, "tds_put_string wrote %d bytes\n", (int) bytes_out);
	return (int)bytes_out;
}

int
tds_put_buf(TDSSOCKET * tds, const unsigned char *buf, int dsize, int ssize)
{
	int cpsize;

	cpsize = ssize > dsize ? dsize : ssize;
	tds_put_n(tds, buf, cpsize);
	dsize -= cpsize;
	tds_put_n(tds, NULL, dsize);
	return tds_put_byte(tds, cpsize);
}

int
tds_put_int8(TDSSOCKET * tds, TDS_INT8 i)
{
#if WORDS_BIGENDIAN
	TDS_UINT h;

	if (tds->emul_little_endian) {
		h = (TDS_UINT) i;
		tds_put_byte(tds, h & 0x000000FF);
		tds_put_byte(tds, (h & 0x0000FF00) >> 8);
		tds_put_byte(tds, (h & 0x00FF0000) >> 16);
		tds_put_byte(tds, (h & 0xFF000000) >> 24);
		h = (TDS_UINT) (i >> 32);
		tds_put_byte(tds, h & 0x000000FF);
		tds_put_byte(tds, (h & 0x0000FF00) >> 8);
		tds_put_byte(tds, (h & 0x00FF0000) >> 16);
		tds_put_byte(tds, (h & 0xFF000000) >> 24);
		return 0;
	}
#endif
	return tds_put_n(tds, (const unsigned char *) &i, sizeof(TDS_INT8));
}

int
tds_put_int(TDSSOCKET * tds, TDS_INT i)
{
#if TDS_ADDITIONAL_SPACE == 0
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_byte(tds, i & 0x000000FF);
		tds_put_byte(tds, (i & 0x0000FF00) >> 8);
		tds_put_byte(tds, (i & 0x00FF0000) >> 16);
		tds_put_byte(tds, (i & 0xFF000000) >> 24);
		return 0;
	}
#endif
	return tds_put_n(tds, (const unsigned char *) &i, sizeof(TDS_INT));
#else
	TDS_UCHAR *p;

	if (tds->out_pos >= tds->env.block_size)
		tds_write_packet(tds, 0x0);

	p = &tds->out_buf[tds->out_pos];
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian)
		TDS_PUT_UA4LE(p, i);
	else
		TDS_PUT_UA4(p, i);
#else
	TDS_PUT_UA4(p, i);
#endif
	tds->out_pos += 4;
	return 0;
#endif
}

int
tds_put_smallint(TDSSOCKET * tds, TDS_SMALLINT si)
{
#if TDS_ADDITIONAL_SPACE == 0
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_byte(tds, si & 0x000000FF);
		tds_put_byte(tds, (si & 0x0000FF00) >> 8);
		return 0;
	}
#endif
	return tds_put_n(tds, (const unsigned char *) &si, sizeof(TDS_SMALLINT));
#else
	TDS_UCHAR *p;

	if (tds->out_pos >= tds->env.block_size)
		tds_write_packet(tds, 0x0);

	p = &tds->out_buf[tds->out_pos];
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian)
		TDS_PUT_UA2LE(p, si);
	else
		TDS_PUT_UA2(p, si);
#else
	TDS_PUT_UA2(p, si);
#endif
	tds->out_pos += 2;
	return 0;
#endif
}

int
tds_put_byte(TDSSOCKET * tds, unsigned char c)
{
	if (tds->out_pos >= (unsigned int)tds->env.block_size)
		tds_write_packet(tds, 0x0);
	tds->out_buf[tds->out_pos++] = c;
	return 0;
}

int
tds_init_write_buf(TDSSOCKET * tds)
{
	/* TODO needed ?? */
	memset(tds->out_buf, '\0', tds->env.block_size);
	tds->out_pos = 8;
	return 0;
}

/**
 * Flush packet to server
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_flush_packet(TDSSOCKET * tds)
{
	int result = TDS_FAIL;

	/* GW added check for tds->s */
	if (!IS_TDSDEAD(tds))
		result = tds_write_packet(tds, 0x01);
	return result;
}

/** @} */
