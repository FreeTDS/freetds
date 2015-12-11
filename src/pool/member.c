/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001, 2002, 2003, 2004, 2005  Brian Bruns
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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#include "pool.h"
#include "replacements.h"
#include <freetds/string.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif /* MAXHOSTNAMELEN */

static TDSSOCKET *pool_mbr_login(TDS_POOL * pool);

/*
 * pool_mbr_login open a single pool login, to be call at init time or
 * to reconnect.
 */
static TDSSOCKET *
pool_mbr_login(TDS_POOL * pool)
{
	TDSCONTEXT *context;
	TDSLOGIN *login;
	TDSSOCKET *tds;
	TDSLOGIN *connection;
	char hostname[MAXHOSTNAMELEN];

	login = tds_alloc_login(1);
	if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
		strlcpy(hostname, "tdspool", MAXHOSTNAMELEN);
	if (!tds_set_passwd(login, pool->password)
	    || !tds_set_user(login, pool->user)
	    || !tds_set_app(login, "tdspool")
	    || !tds_set_host(login, hostname)
	    || !tds_set_library(login, "TDS-Library")
	    || !tds_set_server(login, pool->server)
	    || !tds_set_client_charset(login, "iso_1")
	    || !tds_set_language(login, "us_english")) {
		tds_free_login(login);
		return NULL;
	}
	if (pool->database && strlen(pool->database)) {
		if (!tds_dstr_copy(&login->database, pool->database)) {
			tds_free_login(login);
			return NULL;
		}
	}
	context = tds_alloc_context(NULL);
	tds = tds_alloc_socket(context, 512);
	connection = tds_read_config_info(tds, login, context->locale);
	if (!connection || TDS_FAILED(tds_connect_and_login(tds, connection))) {
		tds_free_socket(tds);
		tds_free_login(connection);
		/* what to do? */
		fprintf(stderr, "Could not open connection to server %s\n", pool->server);
		return NULL;
	}
	tds_free_login(connection);

	if (pool->database && strlen(pool->database)) {
		if (strcasecmp(tds->conn->env.database, pool->database) != 0) {
			fprintf(stderr, "changing database failed\n");
			return NULL;
		}
	}

	return tds;
}

void
pool_assign_member(TDS_POOL_MEMBER * pmbr, TDS_POOL_USER *puser)
{
	assert(pmbr->current_user == NULL);
	if (pmbr->current_user)
		pmbr->current_user->assigned_member = NULL;
	pmbr->current_user = puser;
	puser->assigned_member = pmbr;
}

void
pool_deassign_member(TDS_POOL_MEMBER * pmbr)
{
	if (pmbr->current_user)
		pmbr->current_user->assigned_member = NULL;
	pmbr->current_user = NULL;
	pmbr->state = TDS_IDLE;
}

/*
 * if a dead connection on the client side left this member in a questionable
 * state, let's bring in a correct one
 * We are not sure what the client did so we must try to clean as much as
 * possible.
 * Use pool_free_member if the state is really broken.
 */
void
pool_reset_member(TDS_POOL_MEMBER * pmbr)
{
	// FIXME not wait for server !!! asyncronous
	TDSSOCKET *tds = pmbr->tds;

	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
		pool_free_user(pmbr->current_user);
		pmbr->current_user = NULL;
	}

	/* cancel whatever pending */
	tds->state = TDS_IDLE;
	tds_init_write_buf(tds);
	tds->out_flag = TDS_CANCEL;
	tds_flush_packet(tds);
	tds->state = TDS_PENDING;

	if (tds_read_packet(tds) < 0) {
		pool_free_member(pmbr);
		return;
	}

	if (IS_TDS71_PLUS(tds->conn)) {
		/* this 0x9 final reset the state from mssql 2000 */
		tds_init_write_buf(tds);
		tds->out_flag = TDS_QUERY;
		tds_write_packet(tds, 0x9);
		tds->state = TDS_PENDING;

		if (tds_read_packet(tds) < 0) {
			pool_free_member(pmbr);
			return;
		}
	}

	pmbr->state = TDS_IDLE;
}

void
pool_free_member(TDS_POOL_MEMBER * pmbr)
{
	TDSSOCKET *tds = pmbr->tds;
	if (!IS_TDSDEAD(tds))
		tds_close_socket(tds);
	if (tds)
		tds_free_socket(tds);
	pmbr->tds = NULL;
	/*
	 * if he is allocated disconnect the client 
	 * otherwise we end up with broken client.
	 */
	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
		pool_free_user(pmbr->current_user);
		pmbr->current_user = NULL;
	}
	memset(pmbr, 0, sizeof(*pmbr));
	pmbr->state = TDS_IDLE;
}

void
pool_mbr_init(TDS_POOL * pool)
{
	TDS_POOL_MEMBER *pmbr;
	int i;

	/* allocate room for pool members */

	pool->members = (TDS_POOL_MEMBER *)
		calloc(pool->num_members, sizeof(TDS_POOL_MEMBER));

	/* open connections for each member */

	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];
		pmbr->state = TDS_IDLE;
		if (i >= pool->min_open_conn)
			continue;

		pmbr->tds = pool_mbr_login(pool);
		pmbr->last_used_tm = time(NULL);
		if (!pmbr->tds) {
			fprintf(stderr, "Could not open initial connection %d\n", i);
			exit(1);
		}
		if (!IS_TDS71_PLUS(pmbr->tds->conn)) {
			fprintf(stderr, "Current pool implementation does not support protocol versions former than 7.1\n");
			exit(1);
		}
	}
}

/* 
 * pool_process_members
 * check the fd_set for members returning data to the client, lookup the 
 * client holding this member and forward the results.
 */
int
pool_process_members(TDS_POOL * pool, fd_set * fds)
{
	TDS_POOL_MEMBER *pmbr;
	TDS_POOL_USER *puser;
	TDSSOCKET *tds;
	int i, age, ret;
	int cnt = 0;
	time_t time_now;

	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];

		tds = pmbr->tds;
		if (!tds) {
			assert(pmbr->state == TDS_IDLE);
			continue;	/* dead connection */
		}

		time_now = time(NULL);
		if (FD_ISSET(tds_get_s(tds), fds)) {
			pmbr->last_used_tm = time_now;
			cnt++;
			if (pool_packet_read(tds))
				continue;

			if (tds->in_len == 0) {
				fprintf(stderr, "Uh oh! member %d disconnected\n", i);
				/* mark as dead */
				pool_free_member(pmbr);
			} else if (tds->in_len < 0) {
				fprintf(stderr, "Uh oh! member %d disconnected\n", i);
				perror("read");
				pool_free_member(pmbr);
			} else {
				tdsdump_dump_buf(TDS_DBG_NETWORK, "Got packet from server:", tds->in_buf, tds->in_len);
				/* fprintf(stderr, "read %d bytes from member %d\n", tds->in_len, i); */
				puser = pmbr->current_user;
				if (puser) {
					tdsdump_log(TDS_DBG_INFO1, "writing it sock %d\n", tds_get_s(puser->tds));
					/* cf. net.c for better technique.  */
					/* FIXME handle partial write, stop read on member */
					ret = pool_write_all(tds_get_s(puser->tds), tds->in_buf, tds->in_len);
					if (ret < 0) { /* couldn't write, ditch the user */
						fprintf(stdout, "member %d received error while writing\n",i);
						pool_free_member(pmbr);
					}
				}
			}
		}
		age = time_now - pmbr->last_used_tm;
		if (age > pool->max_member_age && i >= pool->min_open_conn && !pmbr->current_user) {
			fprintf(stderr, "member %d is %d seconds old...closing\n", i, age);
			pool_free_member(pmbr);
		}
	}
	return cnt;
}

/*
 * pool_find_idle_member
 * returns the first pool member in TDS_IDLE state
 */
TDS_POOL_MEMBER *
pool_find_idle_member(TDS_POOL * pool)
{
	int i, active_members;
	TDS_POOL_MEMBER *pmbr;

	active_members = 0;
	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];
		if (pmbr->tds) {
			active_members++;
			if (pmbr->current_user == NULL) {
				/*
				 * make sure member wasn't idle more that the timeout 
				 * otherwise it'll send the query and close leaving a
				 * hung client
				 */
				pmbr->last_used_tm = time(NULL);
				return pmbr;
			}
		}
	}
	/* if we have dead connections we can open */
	if (active_members < pool->num_members) {
		pmbr = NULL;
		for (i = 0; i < pool->num_members; i++) {
			pmbr = &pool->members[i];
			if (!pmbr->tds) {
				fprintf(stderr, "No open connections left, opening member number %d\n", i);
				pmbr->tds = pool_mbr_login(pool);
				if (pmbr->tds && !IS_TDS71_PLUS(pmbr->tds->conn)) {
					tds_free_socket(pmbr->tds);
					pmbr->tds = NULL;
				}
				pmbr->last_used_tm = time(NULL);
				break;
			}
		}
		if (pmbr)
			return pmbr;
	}
	fprintf(stderr, "No idle members left, increase MAX_POOL_CONN\n");
	return NULL;
}

