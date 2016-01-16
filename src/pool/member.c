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
#include <freetds/utils/string.h>

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

	login = tds_alloc_login(true);
	if (!login) {
		fprintf(stderr, "out of memory");
		return NULL;
	}
	if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
		strlcpy(hostname, "tdspool", MAXHOSTNAMELEN);
	if (!tds_set_passwd(login, pool->server_password)
	    || !tds_set_user(login, pool->server_user)
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
	if (!context) {
		fprintf(stderr, "Context cannot be null\n");
		return NULL;
	}
	tds = tds_alloc_socket(context, 512);
	if (!tds) {
		fprintf(stderr, "tds cannot be null\n");
		return NULL;
	}
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
pool_assign_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr, TDS_POOL_USER *puser)
{
	pool_mbr_check(pool);
	assert(pmbr->current_user == NULL);
	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
	} else {
		dlist_member_remove(&pool->idle_members, pmbr);
		dlist_member_append(&pool->active_members, pmbr);
	}
	pmbr->current_user = puser;
	puser->assigned_member = pmbr;
	pool_mbr_check(pool);
}

void
pool_deassign_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr)
{
	if (pmbr->current_user) {
		pmbr->current_user->assigned_member = NULL;
		pmbr->current_user = NULL;
		dlist_member_remove(&pool->active_members, pmbr);
		dlist_member_append(&pool->idle_members, pmbr);
	}
	pmbr->sock.poll_send = false;
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
	TDSSOCKET *tds = pmbr->sock.tds;
	TDS_POOL_USER *puser;

	puser = pmbr->current_user;
	if (puser) {
		pool_deassign_member(pool, pmbr);
		pool_free_user(pool, puser);
	}

	/* cancel whatever pending */
	tds_init_write_buf(tds);
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		goto failure;
	tds->out_flag = TDS_CANCEL;
	if (TDS_FAILED(tds_flush_packet(tds)))
		goto failure;
	tds_set_state(tds, TDS_PENDING);
	tds->in_cancel = 2;

	if (TDS_FAILED(tds_process_cancel(tds)))
		goto failure;

	if (IS_TDS71_PLUS(tds->conn)) {
		/* this 0x9 final reset the state from mssql 2000 */
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
			goto failure;
		tds_start_query(tds, TDS_QUERY);
		tds_put_string(tds, "WHILE @@TRANCOUNT > 0 ROLLBACK SET TRANSACTION ISOLATION LEVEL READ COMMITTED", -1);
		tds_write_packet(tds, 0x9);
		tds_set_state(tds, TDS_PENDING);

		if (TDS_FAILED(tds_process_simple_query(tds)))
			goto failure;
	}
	return;

failure:
	pool_free_member(pool, pmbr);
}

void
pool_free_member(TDS_POOL * pool, TDS_POOL_MEMBER * pmbr)
{
	TDSSOCKET *tds;
	TDS_POOL_USER *puser;

	tds = pmbr->sock.tds;
	if (tds) {
		if (!IS_TDSDEAD(tds))
			tds_close_socket(tds);
		pool_mbr_free_socket(tds);
		pmbr->sock.tds = NULL;
	}

	/*
	 * if he is allocated disconnect the client 
	 * otherwise we end up with broken client.
	 */
	puser = pmbr->current_user;
	if (puser) {
		pool_deassign_member(pool, pmbr);
		pool_free_user(pool, puser);
	}

	if (dlist_member_in_list(&pool->active_members, pmbr)) {
		pool->num_active_members--;
		dlist_member_remove(&pool->active_members, pmbr);
	}
	free(pmbr);
	pool_mbr_check(pool);
}

void
pool_mbr_init(TDS_POOL * pool)
{
	TDS_POOL_MEMBER *pmbr;

	/* allocate room for pool members */

	pool->num_active_members = 0;
	dlist_member_init(&pool->active_members);
	dlist_member_init(&pool->idle_members);
	pool_mbr_check(pool);

	/* open connections for each member */
	while (pool->num_active_members < pool->min_open_conn) {
		pmbr = tds_new0(TDS_POOL_MEMBER, 1);
		if (!pmbr) {
			fprintf(stderr, "Out of memory\n");
			exit(1);
		}
		pmbr->sock.poll_recv = true;

		pmbr->sock.tds = pool_mbr_login(pool, 0);
		if (!pmbr->sock.tds) {
			fprintf(stderr, "Could not open initial connection\n");
			exit(1);
		}
		pmbr->last_used_tm = time(NULL);
		pool->num_active_members++;
		dlist_member_append(&pool->idle_members, pmbr);
		if (!IS_TDS71_PLUS(pmbr->sock.tds->conn)) {
			fprintf(stderr, "Current pool implementation does not support protocol versions former than 7.1\n");
			exit(1);
		}
		pool->member_logins++;
	}
	pool_mbr_check(pool);
}

void
pool_mbr_destroy(TDS_POOL * pool)
{
	while (dlist_member_first(&pool->active_members))
		pool_free_member(pool, dlist_member_first(&pool->active_members));
	while (dlist_member_first(&pool->idle_members))
		pool_free_member(pool, dlist_member_first(&pool->idle_members));

	assert(pool->num_active_members == 0);
	pool->num_active_members = 0;
}

static bool
pool_process_data(TDS_POOL *pool, TDS_POOL_MEMBER *pmbr)
{
	TDSSOCKET *tds = pmbr->sock.tds;
	TDS_POOL_USER *puser = NULL;

	for (;;) {
		if (pool_packet_read(tds))
			break;

		/* disconnected */
		if (tds->in_len == 0) {
			tdsdump_log(TDS_DBG_INFO1, "Uh oh! member disconnected\n");
			/* mark as dead */
			pool_free_member(pool, pmbr);
			return false;
		}

		tdsdump_dump_buf(TDS_DBG_NETWORK, "Got packet from server:", tds->in_buf, tds->in_len);
		puser = pmbr->current_user;
		if (!puser)
			break;

		tdsdump_log(TDS_DBG_INFO1, "writing it sock %d\n", (int) tds_get_s(puser->sock.tds));
		if (!pool_write_data(&pmbr->sock, &puser->sock)) {
			tdsdump_log(TDS_DBG_ERROR, "member received error while writing\n");
			pool_free_user(pool, puser);
			return false;
		}
		if (tds->in_pos < tds->in_len)
			/* partial write, schedule a future write */
			break;
	}
	if (puser && !puser->sock.poll_send)
		tds_socket_flush(tds_get_s(puser->sock.tds));
	return true;
}

/* 
 * pool_process_members
 * check the fd_set for members returning data to the client, lookup the 
 * client holding this member and forward the results.
 * @return Timeout you should call this function again or -1 for infinite
 */
int
pool_process_members(TDS_POOL * pool, struct pollfd *fds, unsigned num_fds)
{
	TDS_POOL_MEMBER *pmbr, *next;
	time_t age;
	time_t time_now;
	int min_expire_left = -1;
	short revents;

	pool_mbr_check(pool);
	for (next = dlist_member_first(&pool->active_members); (pmbr = next) != NULL; ) {
		bool processed = false;

		next = dlist_member_next(&pool->active_members, pmbr);

		assert(pmbr->current_user);
		if (pmbr->doing_async || pmbr->sock.poll_index > num_fds)
			continue;

		revents = fds[pmbr->sock.poll_index].revents;
		assert(pmbr->sock.tds);

		time_now = time(NULL);
		if (pmbr->sock.poll_recv && (revents & (POLLIN|POLLHUP)) != 0) {
			if (!pool_process_data(pool, pmbr))
				continue;
			processed = true;
		}
		if (pmbr->sock.poll_send && (revents & POLLOUT) != 0) {
			if (!pool_write_data(&pmbr->current_user->sock, &pmbr->sock)) {
				pool_free_member(pool, pmbr);
				continue;
			}
			processed = true;
		}
		if (processed)
			pmbr->last_used_tm = time_now;
	}

	if (pool->num_active_members <= pool->min_open_conn)
		return min_expire_left;

	/* close old connections */
	time_now = time(NULL);
	for (next = dlist_member_first(&pool->idle_members); (pmbr = next) != NULL; ) {

		next = dlist_member_next(&pool->idle_members, pmbr);

		assert(pmbr->sock.tds);
		assert(!pmbr->current_user);

		age = time_now - pmbr->last_used_tm;
		if (age >= pool->max_member_age) {
			tdsdump_log(TDS_DBG_INFO1, "member is %ld seconds old...closing\n", (long int) age);
			pool_free_member(pool, pmbr);
		} else {
			int left = (int) (pool->max_member_age - age);
			if (min_expire_left < 0 || left < min_expire_left)
				min_expire_left = left;
		}
	}
	return min_expire_left;
}

static bool
compatible_versions(const TDSSOCKET *tds, const TDS_POOL_USER *user)
{
	if (tds->conn->tds_version != user->login->tds_version)
		return false;
	return true;
}

typedef struct {
	TDS_POOL_EVENT common;
	TDS_POOL *pool;
	TDS_POOL_MEMBER *pmbr;
	int tds_version;
} CONNECT_EVENT;

static void connect_execute_ok(TDS_POOL_EVENT *base_event);
static void connect_execute_ko(TDS_POOL_EVENT *base_event);

static TDS_THREAD_PROC_DECLARE(connect_proc, arg)
{
	CONNECT_EVENT *ev = (CONNECT_EVENT *) arg;
	TDS_POOL_MEMBER *pmbr = ev->pmbr;
	TDS_POOL *pool = ev->pool;

	for (;;) {
		pmbr->sock.tds = pool_mbr_login(pool, ev->tds_version);
		if (!pmbr->sock.tds) {
			tdsdump_log(TDS_DBG_ERROR, "Error opening a new connection to server\n");
			break;
		}
		if (!IS_TDS71_PLUS(pmbr->sock.tds->conn)) {
			tdsdump_log(TDS_DBG_ERROR, "Protocol server version not supported\n");
			break;
		}

		/* if already attached to a user we can send login directly */
		if (pmbr->current_user)
			if (!pool_user_send_login_ack(pool, pmbr->current_user))
				break;

		pool_event_add(pool, &ev->common, connect_execute_ok);
		return TDS_THREAD_RESULT(0);
	}

	/* failure */
	pool_event_add(pool, &ev->common, connect_execute_ko);
	return TDS_THREAD_RESULT(0);
}

static void
connect_execute_ko(TDS_POOL_EVENT *base_event)
{
	CONNECT_EVENT *ev = (CONNECT_EVENT *) base_event;

	pool_free_member(ev->pool, ev->pmbr);
}

static void
connect_execute_ok(TDS_POOL_EVENT *base_event)
{
	CONNECT_EVENT *ev = (CONNECT_EVENT *) base_event;
	TDS_POOL_MEMBER *pmbr = ev->pmbr;
	TDS_POOL_USER *puser = pmbr->current_user;

	ev->pool->member_logins++;
	pmbr->doing_async = false;

	pmbr->last_used_tm = time(NULL);

	if (puser) {
		pmbr->sock.poll_recv = true;
		puser->sock.poll_recv = true;

		puser->user_state = TDS_SRV_QUERY;
	}
}

/*
 * pool_assign_idle_member
 * assign a member to the user specified
 */
TDS_POOL_MEMBER *
pool_assign_idle_member(TDS_POOL * pool, TDS_POOL_USER *puser)
{
	TDS_POOL_MEMBER *pmbr;
	CONNECT_EVENT *ev;

	puser->sock.poll_recv = false;
	puser->sock.poll_send = false;

	pool_mbr_check(pool);
	DLIST_FOREACH(dlist_member, &pool->idle_members, pmbr) {
		assert(pmbr->current_user == NULL);
		assert(!pmbr->doing_async);

		assert(pmbr->sock.tds);

		if (!compatible_versions(pmbr->sock.tds, puser))
			continue;

		pool_assign_member(pool, pmbr, puser);

		/*
		 * make sure member wasn't idle more that the timeout
		 * otherwise it'll send the query and close leaving a
		 * hung client
		 */
		pmbr->last_used_tm = time(NULL);
		pmbr->sock.poll_recv = false;
		pmbr->sock.poll_send = false;

		pool_user_finish_login(pool, puser);
		return pmbr;
	}

	/* if we can open a new connection open it */
	if (pool->num_active_members >= pool->max_open_conn) {
		fprintf(stderr, "No idle members left, increase \"max pool conn\"\n");
		return NULL;
	}

	pmbr = tds_new0(TDS_POOL_MEMBER, 1);
	if (!pmbr) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	tdsdump_log(TDS_DBG_INFO1, "No open connections left, opening new member\n");

	ev = tds_new0(CONNECT_EVENT, 1);
	if (!ev) {
		free(pmbr);
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}
	ev->pmbr = pmbr;
	ev->pool = pool;
	ev->tds_version = puser->login->tds_version;

	if (tds_thread_create_detached(connect_proc, ev) != 0) {
		free(pmbr);
		free(ev);
		fprintf(stderr, "error creating thread\n");
		return NULL;
	}
	pmbr->doing_async = true;

	pool_mbr_check(pool);
	pool->num_active_members++;
	dlist_member_append(&pool->idle_members, pmbr);
	pool_mbr_check(pool);

	pool_assign_member(pool, pmbr, puser);
	puser->sock.poll_send = false;
	puser->sock.poll_recv = false;

	return pmbr;
}

#if ENABLE_EXTRA_CHECKS
void pool_mbr_check(TDS_POOL *pool)
{
	TDS_POOL_MEMBER *pmbr;
	unsigned total = 0;

	DLIST_FOREACH(dlist_member, &pool->active_members, pmbr) {
		assert(pmbr->doing_async || pmbr->sock.tds);
		assert(pmbr->current_user);
		++total;
	}
	DLIST_FOREACH(dlist_member, &pool->idle_members, pmbr) {
		assert(pmbr->doing_async || pmbr->sock.tds);
		assert(!pmbr->current_user);
		++total;
	}
	assert(pool->num_active_members == total);
}
#endif
