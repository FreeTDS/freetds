/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <ctype.h>

#include "pool.h"
#include <freetds/string.h>
#include <freetds/checks.h>
#include <freetds/bytes.h>

void
dump_login(TDSLOGIN * login)
{
	fprintf(stderr, "host %s\n", tds_dstr_cstr(&login->client_host_name));
	fprintf(stderr, "user %s\n", tds_dstr_cstr(&login->user_name));
	fprintf(stderr, "pass %s\n", tds_dstr_cstr(&login->password));
	fprintf(stderr, "app  %s\n", tds_dstr_cstr(&login->app_name));
	fprintf(stderr, "srvr %s\n", tds_dstr_cstr(&login->server_name));
	fprintf(stderr, "vers %d.%d\n", TDS_MAJOR(login), TDS_MINOR(login));
	fprintf(stderr, "lib  %s\n", tds_dstr_cstr(&login->library));
	fprintf(stderr, "lang %s\n", tds_dstr_cstr(&login->language));
	fprintf(stderr, "char %s\n", tds_dstr_cstr(&login->server_charset));
	fprintf(stderr, "bsiz %d\n", login->block_size);
}

/**
 * Read part of packet. Function does not block.
 * @return true if packet is not complete and we must call again,
 *         false on full packet or error.
 */
bool
pool_packet_read(TDSSOCKET *tds)
{
	int packet_len;
	int readed, err;

	tdsdump_log(TDS_DBG_INFO1, "tds in_len %d in_pos %d\n", tds->in_len, tds->in_pos);

	// TODO MARS

	/* determine packet size */
	packet_len = tds->in_len >= 4 ? TDS_GET_A2BE(&tds->in_buf[2]) : 8;
	if (TDS_UNLIKELY(packet_len < 8)) {
		tds->in_len = 0;
		return false;
	}

	/* get another packet */
	if (tds->in_len >= packet_len) {
		/* packet was not fully forwarded */
		if (tds->in_pos < tds->in_len)
			return false;
		tds->in_pos = 0;
		tds->in_len = 0;
	}

	for (;;) {
		/* determine packet size */
		packet_len = 8;
		if (tds->in_len >= 4) {
			packet_len = TDS_GET_A2BE(&tds->in_buf[2]);
			if (packet_len < 8)
				break;
			tdsdump_log(TDS_DBG_INFO1, "packet_len %d in_len %d\n", packet_len, tds->in_len);
			/* resize packet if not enough */
			if (packet_len > tds->recv_packet->capacity) {
				TDSPACKET *packet;

				packet = tds_realloc_packet(tds->recv_packet, packet_len);
				if (!packet)
					break;
				tds->in_buf = packet->buf;
				tds->recv_packet = packet;
			}
			CHECK_TDS_EXTRA(tds);
			if (tds->in_len >= packet_len)
				return false;
		}

		assert(packet_len > tds->in_len);
		assert(packet_len <= tds->recv_packet->capacity);
		assert(tds->in_len < tds->recv_packet->capacity);

		readed = read(tds_get_s(tds), &tds->in_buf[tds->in_len], packet_len - tds->in_len);
		tdsdump_log(TDS_DBG_INFO1, "readed %d\n", readed);

		/* socket closed */
		if (readed == 0)
			break;

		/* error */
		if (readed < 0) {
			err = sock_errno;
			if (err == EINTR)
				continue;
			if (TDSSOCK_WOULDBLOCK(err))
				return true;
			break;
		}

		/* got some data */
		tds->in_len += readed;
	}

	/* failure */
	tds->in_len = 0;
	return false;
}

int
pool_write(TDS_SYS_SOCKET sock, const void *buf, size_t len)
{
	int ret;
	const unsigned char *p = (const unsigned char *) buf;

	while (len) {
		ret = WRITESOCKET(sock, p, len);
		if (ret <= 0) {
			int err = errno;
			if (TDSSOCK_WOULDBLOCK(err) || err == EINTR)
				break;
			return -1;
		}
		p   += ret;
		len -= ret;
	}
	return p - (const unsigned char *) buf;
}

void
pool_event_add(TDS_POOL *pool, TDS_POOL_EVENT *ev, TDS_POOL_EXECUTE execute)
{
	tds_mutex_lock(&pool->events_mtx);
	ev->execute = execute;
	ev->next = pool->events;
	pool->events = ev;
	tds_mutex_unlock(&pool->events_mtx);
	WRITESOCKET(pool->event_fd, "x", 1);
}

bool
pool_write_data(TDS_POOL_SOCKET *from, TDS_POOL_SOCKET *to)
{
	int ret;
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_INFO1, "trying to send\n");

	tds = from->tds;
	tdsdump_log(TDS_DBG_INFO1, "sending %d bytes\n", tds->in_len);
	/* cf. net.c for better technique.  */
	ret = pool_write(tds_get_s(to->tds), tds->in_buf + tds->in_pos, tds->in_len - tds->in_pos);
	/* write failed, cleanup member */
	if (ret < 0)
		return false;

	tds->in_pos += ret;
	if (tds->in_pos < tds->in_len) {
		/* partial write, schedule a future write */
		to->poll_send = true;
		from->poll_recv = false;
	} else {
		to->poll_send = false;
		from->poll_recv = true;
	}
	return true;
}
