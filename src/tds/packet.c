/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2012 Frediano Ziglio
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
#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**
 * \addtogroup network
 * @{ 
 */

/**
 * Read in one 'packet' from the server.  This is a wrapped outer packet of
 * the protocol (they bundle result packets into chunks and wrap them at
 * what appears to be 512 bytes regardless of how that breaks internal packet
 * up.   (tetherow\@nol.org)
 * @return bytes read or -1 on failure
 */
int
tds_read_packet(TDSSOCKET * tds)
{
	unsigned char *pkt = tds->in_buf, *p, *end;

	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_NETWORK, "Read attempt when state is TDS_DEAD");
		return -1;
	}

	tds->in_len = 0;
	tds->in_pos = 0;
	for (p = pkt, end = p+8; p < end;) {
		int len = tds_connection_read(tds, p, end - p);
		if (len <= 0) {
			tds_close_socket(tds);
			return -1;
		}

		p += len;
		if (p - pkt >= 4) {
			unsigned pktlen = pkt[2] * 256u + pkt[3];
			/* packet must at least contains header */
			if (TDS_UNLIKELY(pktlen < 8)) {
				tds_close_socket(tds);
				return -1;
			}
			if (TDS_UNLIKELY(pktlen > tds->in_buf_max)) {
				pkt = (unsigned char *) realloc(tds->in_buf, pktlen);
				if (TDS_UNLIKELY(!pkt)) {
					tds_close_socket(tds);
					return -1;
				}
				p = pkt + (p-tds->in_buf);
				tds->in_buf = pkt;
				/* Set the new maximum packet size */
				tds->in_buf_max = pktlen;
			}
			end = pkt + pktlen;
		}
	}

	/* set the received packet type flag */
	tds->in_flag = pkt[0];

	/* Set the length and pos (not sure what pos is used for now */
	tds->in_len = p - pkt;
	tds->in_pos = 8;
	tdsdump_dump_buf(TDS_DBG_NETWORK, "Received packet", tds->in_buf, tds->in_len);

	return tds->in_len;
}

TDSRET
tds_write_packet(TDSSOCKET * tds, unsigned char final)
{
	int sent;
	unsigned int left = 0;

#if TDS_ADDITIONAL_SPACE != 0
	if (tds->out_pos > tds->env.block_size) {
		left = tds->out_pos - tds->env.block_size;
		tds->out_pos = tds->env.block_size;
	}
#endif

	tds->out_buf[0] = tds->out_flag;
	tds->out_buf[1] = final;
	tds->out_buf[2] = (tds->out_pos) / 256u;
	tds->out_buf[3] = (tds->out_pos) % 256u;
	if (IS_TDS7_PLUS(tds) && !tds->login)
		tds->out_buf[6] = 0x01;

	tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", tds->out_buf, tds->out_pos);

	sent = tds_connection_write(tds, tds->out_buf, tds->out_pos, final);


#if TDS_ADDITIONAL_SPACE != 0
	memcpy(tds->out_buf + 8, tds->out_buf + tds->env.block_size, left);
#endif
	tds->out_pos = left + 8;

	/* GW added in check for write() returning <0 and SIGPIPE checking */
	return sent <= 0 ? TDS_FAIL : TDS_SUCCESS;
}

int
tds_put_cancel(TDSSOCKET * tds)
{
	unsigned char out_buf[8];
	int sent;

	memset(out_buf, 0, sizeof(out_buf));
	out_buf[0] = TDS_CANCEL;	/* out_flag */
	out_buf[1] = 1;	/* final */
	out_buf[2] = 0;
	out_buf[3] = 8;
	if (IS_TDS7_PLUS(tds) && !tds->login)
		out_buf[6] = 0x01;

	tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", out_buf, 8);

	sent = tds_connection_write(tds, out_buf, 8, 1);

	if (sent > 0)
		tds->in_cancel = 1;

	/* GW added in check for write() returning <0 and SIGPIPE checking */
	return sent <= 0 ? TDS_FAIL : TDS_SUCCESS;
}

/** @} */
