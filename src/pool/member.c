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

static void
pool_mbr_free_socket(TDSSOCKET *tds)
{
	if (tds) {
		TDSCONTEXT *ctx = (TDSCONTEXT *) tds->conn->tds_ctx;

		tds_free_socket(tds);
		tds_free_context(ctx);
	}
}

/*
 * pool_mbr_login open a single pool login, to be call at init time or
 * to reconnect.
 */
static TDSSOCKET *
pool_mbr_login(const TDS_POOL * pool, int tds_version)
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
	if (tds_version > 0)
		login->tds_version = tds_version;
	if (pool->database && strlen(pool->database)) {
		if (!tds_dstr_copy(&login->database, pool->database)) {
			tds_free_login(login);
			return NULL;
		}
	}
	context = tds_alloc_context(NULL);
	tds = tds_alloc_socket(context, 512);
	connection = tds_read_config_info(tds, login, context->locale);
	tds_free_login(login);
	if (!connection || TDS_FAILED(tds_connect_and_login(tds, connection))) {
		pool_mbr_free_socket(tds);
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
	pmbr->poll_recv = true;
	pmbr->poll_send = false;
	puser->assigned_member = pmbr;
}

void
pool_deassign_member(TDS_POOL_MEMBER * pmbr)
{
	if (pmbr->current_user)
		pmbr->current_user->assigned_member = NULL;
	pmbr->current_user = NULL;
	pmbr->poll_send = false;
}

/*
 * if a dead connection on the client side left this member in a questionable
 * state, let's bring in a correct one
 * We are not sure what the client did so we must try to clean as much as
 * possible.
 * Use pool_free_member if the state is really broken.
 */
void
pool_reset_member(TDS_POOL * pool, TDS_POOL_MEMBER * pmbr)
{
	// FIXME not wait for server !!! asyncronous
	TDSSOCKET *tds = pmbr->tds;

	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
		pool_free_user(pool, pmbr->current_user);
		pmbr->current_user = NULL;
	}

	/* cancel whatever pending */
	tds->state = TDS_IDLE;
	tds_init_write_buf(tds);
	tds->out_flag = TDS_CANCEL;
	tds_flush_packet(tds);
	tds->state = TDS_PENDING;

	if (tds_read_packet(tds) < 0) {
		pool_free_member(pool, pmbr);
		return;
	}

	if (IS_TDS71_PLUS(tds->conn)) {
		/* this 0x9 final reset the state from mssql 2000 */
		tds_init_write_buf(tds);
		tds->out_flag = TDS_QUERY;
		tds_write_packet(tds, 0x9);
		tds->state = TDS_PENDING;

		if (tds_read_packet(tds) < 0) {
			pool_free_member(pool, pmbr);
			return;
		}
	}
}

void
pool_free_member(TDS_POOL * pool, TDS_POOL_MEMBER * pmbr)
{
	TDSSOCKET *tds = pmbr->tds;
	if (!IS_TDSDEAD(tds))
		tds_close_socket(tds);
	if (tds) {
		pool_mbr_free_socket(tds);
		pool->active_members--;
	}
	pmbr->tds = NULL;
	/*
	 * if he is allocated disconnect the client 
	 * otherwise we end up with broken client.
	 */
	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
		pool_free_user(pool, pmbr->current_user);
		pmbr->current_user = NULL;
	}
	memset(pmbr, 0, sizeof(*pmbr));
}

void
pool_mbr_init(TDS_POOL * pool)
{
	TDS_POOL_MEMBER *pmbr;
	int i;

	/* allocate room for pool members */

	pool->active_members = 0;
	pool->members = (TDS_POOL_MEMBER *)
		calloc(pool->num_members, sizeof(TDS_POOL_MEMBER));

	/* open connections for each member */
	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];
		if (i >= pool->min_open_conn)
			continue;

		pmbr->poll_recv = true;

		pmbr->tds = pool_mbr_login(pool, 0);
		if (!pmbr->tds) {
			fprintf(stderr, "Could not open initial connection %d\n", i);
			exit(1);
		}
		pmbr->last_used_tm = time(NULL);
		pool->active_members++;
		if (!IS_TDS71_PLUS(pmbr->tds->conn)) {
			fprintf(stderr, "Current pool implementation does not support protocol versions former than 7.1\n");
			exit(1);
		}
	}
}

void
pool_mbr_destroy(TDS_POOL * pool)
{
	TDS_POOL_MEMBER *pmbr;
	int i;

	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];
		if (!IS_TDSDEAD(pmbr->tds)) {
			fprintf(stderr, "Closing member %d\n", i);
			tds_close_socket(pmbr->tds);
		}
		if (pmbr->tds) {
			pool_mbr_free_socket(pmbr->tds);
			pmbr->tds = NULL;
		}
	}
	free(pool->members);
	pool->members = NULL;
	pool->num_members = 0;
	pool->active_members = 0;
}

static void
pool_mbr_write(TDS_POOL *pool, TDS_POOL_MEMBER *pmbr)
{
	TDSSOCKET *tds = pmbr->tds;
	TDS_POOL_USER *puser;
	int ret;

	puser = pmbr->current_user;
	if (!puser)
		return;

	ret = pool_write(tds_get_s(puser->tds), &tds->in_buf[tds->in_pos], tds->in_len - tds->in_pos);
	if (ret < 0) { /* couldn't write, ditch the user */
		fprintf(stdout, "member received error while writing\n");
		pool_reset_member(pool, pmbr);
		return;
	}

	tds->in_pos += ret;
	if (tds->in_pos < tds->in_len) {
		/* partial write, schedule a future write */
		pmbr->poll_recv = false;
		puser->poll_send = true;
	} else {
		pmbr->poll_recv = true;
		puser->poll_send = false;
	}
}

static void
pool_process_data(TDS_POOL *pool, TDS_POOL_MEMBER *pmbr, int member_index)
{
	TDSSOCKET *tds = pmbr->tds;
	TDS_POOL_USER *puser = NULL;
	int ret;

	for (;;) {
		if (pool_packet_read(tds))
			break;

		/* disconnected */
		if (tds->in_len == 0) {
			fprintf(stderr, "Uh oh! member %d disconnected\n", member_index);
			/* mark as dead */
			pool_free_member(pool, pmbr);
			return;
		}

		tdsdump_dump_buf(TDS_DBG_NETWORK, "Got packet from server:", tds->in_buf, tds->in_len);
		/* fprintf(stderr, "read %d bytes from member %d\n", tds->in_len, member_index); */
		puser = pmbr->current_user;
		if (!puser)
			break;

		tdsdump_log(TDS_DBG_INFO1, "writing it sock %d\n", tds_get_s(puser->tds));
		/* cf. net.c for better technique.  */
		ret = pool_write(tds_get_s(puser->tds), tds->in_buf, tds->in_len);
		if (ret < 0) { /* couldn't write, ditch the user */
			fprintf(stdout, "member %d received error while writing\n", member_index);
			pool_free_member(pool, pmbr);
			puser = NULL;
			return;
		}
		tds->in_pos += ret;
		if (tds->in_pos < tds->in_len) {
			/* partial write, schedule a future write */
			pmbr->poll_recv = false;
			puser->poll_send = true;
			break;
		}
	}
	if (puser && !puser->poll_send)
		tds_socket_flush(tds_get_s(puser->tds));
}

/* 
 * pool_process_members
 * check the fd_set for members returning data to the client, lookup the 
 * client holding this member and forward the results.
 */
void
pool_process_members(TDS_POOL * pool, fd_set * rfds, fd_set * wfds)
{
	TDS_POOL_MEMBER *pmbr;
	TDSSOCKET *tds;
	int i, age;
	time_t time_now;

	for (i = 0; i < pool->num_members; i++) {
		bool processed = false;

		pmbr = &pool->members[i];

		tds = pmbr->tds;
		if (!tds) {
			continue;	/* dead connection */
		}

		time_now = time(NULL);
		if (pmbr->poll_recv && FD_ISSET(tds_get_s(tds), rfds)) {
			pool_process_data(pool, pmbr, i);
			processed = true;
		}
		if (pmbr->poll_send && FD_ISSET(tds_get_s(tds), wfds)) {
			pool_mbr_write(pool, pmbr);
			processed = true;
		}
		if (processed) {
			pmbr->last_used_tm = time_now;
		} else {
			age = time_now - pmbr->last_used_tm;
			if (age > pool->max_member_age
			    && pool->active_members > pool->min_open_conn
			    && !pmbr->current_user) {
				fprintf(stderr, "member %d is %d seconds old...closing\n", i, age);
				pool_free_member(pool, pmbr);
			}
		}
	}
}

static bool
compatible_versions(const TDSSOCKET *tds, const TDS_POOL_USER *user)
{
	if (tds->conn->tds_version != user->login->tds_version)
		return false;
	return true;
}

/*
 * pool_find_idle_member
 * returns the first pool member in TDS_IDLE state
 */
TDS_POOL_MEMBER *
pool_find_idle_member(TDS_POOL * pool, TDS_POOL_USER *user)
{
	int i, first_dead = -1;
	TDS_POOL_MEMBER *pmbr;

	for (i = 0; i < pool->num_members; i++) {
		pmbr = &pool->members[i];
		if (!pmbr->tds) {
			if (first_dead < 0)
				first_dead = i;
			continue;
		}

		if (pmbr->current_user)
			continue;

		if (!compatible_versions(pmbr->tds, user))
			continue;

		/*
		 * make sure member wasn't idle more that the timeout
		 * otherwise it'll send the query and close leaving a
		 * hung client
		 */
		pmbr->last_used_tm = time(NULL);
		pmbr->poll_recv = true;
		pmbr->poll_send = false;
		return pmbr;
	}
	/* if we have dead connections we can open */
	i = first_dead;
	if (pool->active_members < pool->num_members && i >= 0) {
		pmbr = &pool->members[i];
		assert(pmbr->tds == NULL);

		fprintf(stderr, "No open connections left, opening member number %d\n", i);
		pmbr->tds = pool_mbr_login(pool, user->login->tds_version);
		if (!pmbr->tds) {
			fprintf(stderr, "Error opening a new connection to server\n");
			return NULL;
		}
		pmbr->poll_recv = true;

		if (IS_TDS71_PLUS(pmbr->tds->conn)) {
			pmbr->last_used_tm = time(NULL);
			pool->active_members++;
			return pmbr;
		}

		pool_mbr_free_socket(pmbr->tds);
		pmbr->tds = NULL;
	}
	fprintf(stderr, "No idle members left, increase \"max pool conn\"\n");
	return NULL;
}

