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
#include <errno.h>
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

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#include "pool.h"
#include <freetds/server.h>
#include <freetds/utils/string.h>

static TDS_POOL_USER *pool_user_find_new(TDS_POOL * pool);
static bool pool_user_login(TDS_POOL * pool, TDS_POOL_USER * puser);
static bool pool_user_read(TDS_POOL * pool, TDS_POOL_USER * puser);
static void login_execute(TDS_POOL_EVENT *base_event);
static void end_login_execute(TDS_POOL_EVENT *base_event);

void
pool_user_init(TDS_POOL * pool)
{
	dlist_user_init(&pool->users);
	dlist_user_init(&pool->waiters);
	pool->ctx = tds_alloc_context(NULL);
}

void
pool_user_destroy(TDS_POOL * pool)
{
	while (dlist_user_first(&pool->users))
		pool_free_user(pool, dlist_user_first(&pool->users));
	while (dlist_user_first(&pool->waiters))
		pool_free_user(pool, dlist_user_first(&pool->waiters));

	tds_free_context(pool->ctx);
	pool->ctx = NULL;
}

static TDS_POOL_USER *
pool_user_find_new(TDS_POOL * pool)
{
	TDS_POOL_USER *puser;

	/* did we exhaust the number of concurrent users? */
	if (pool->num_users >= MAX_POOL_USERS) {
		fprintf(stderr, "Max concurrent users exceeded, increase in pool.h\n");
		return NULL;
	}

	puser = tds_new0(TDS_POOL_USER, 1);
	if (!puser) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	dlist_user_append(&pool->users, puser);
	pool->num_users++;

	return puser;
}

typedef struct {
	TDS_POOL_EVENT common;
	TDS_POOL *pool;
	TDS_POOL_USER *puser;
	bool success;
} LOGIN_EVENT;

static TDS_THREAD_PROC_DECLARE(login_proc, arg)
{
	LOGIN_EVENT *ev = (LOGIN_EVENT *) arg;

	ev->success = pool_user_login(ev->pool, ev->puser);

	pool_event_add(ev->pool, &ev->common, login_execute);
	return TDS_THREAD_RESULT(0);
}

static void
login_execute(TDS_POOL_EVENT *base_event)
{
	LOGIN_EVENT *ev = (LOGIN_EVENT *) base_event;
	TDS_POOL_USER *puser = ev->puser;
	TDS_POOL *pool = ev->pool;

	if (!ev->success) {
		/* login failed...free socket */
		pool_free_user(pool, puser);
		return;
	}

	puser->sock.poll_recv = true;

	/* try to assign a member, connection can have transactions
	 * and so on so deassign only when disconnected */
	pool_user_query(pool, puser);

	tdsdump_log(TDS_DBG_INFO1, "user state %d\n", puser->user_state);

	assert(puser->login || puser->user_state == TDS_SRV_QUERY);
}


/*
 * pool_user_create
 * accepts a client connection and adds it to the users list and returns it
 */
TDS_POOL_USER *
pool_user_create(TDS_POOL * pool, TDS_SYS_SOCKET s)
{
	TDS_POOL_USER *puser;
	TDS_SYS_SOCKET fd;
	TDSSOCKET *tds;
	LOGIN_EVENT *ev;

	tdsdump_log(TDS_DBG_NETWORK, "accepting connection\n");
	if (TDS_IS_SOCKET_INVALID(fd = tds_accept(s, NULL, NULL))) {
		char *errstr = sock_strerror(errno);
		tdsdump_log(TDS_DBG_ERROR, "error calling assert :%s\n", errstr);
		sock_strerror_free(errstr);
		return NULL;
	}

	if (tds_socket_set_nonblocking(fd) != 0) {
		CLOSESOCKET(fd);
		return NULL;
	}

	puser = pool_user_find_new(pool);
	if (!puser) {
		CLOSESOCKET(fd);
		return NULL;
	}

	tds = tds_alloc_socket(pool->ctx, BLOCKSIZ);
	if (!tds) {
		CLOSESOCKET(fd);
		return NULL;
	}
	ev = tds_new0(LOGIN_EVENT, 1);
	if (!ev || TDS_FAILED(tds_iconv_open(tds->conn, "UTF-8", 0))) {
		free(ev);
		tds_free_socket(tds);
		CLOSESOCKET(fd);
		return NULL;
	}
	tds_set_s(tds, fd);
	tds->state = TDS_IDLE;
	tds->out_flag = TDS_LOGIN;

	puser->sock.tds = tds;
	puser->user_state = TDS_SRV_QUERY;
	puser->sock.poll_recv = false;
	puser->sock.poll_send = false;

	/* launch login asyncronously */
	ev->puser = puser;
	ev->pool = pool;

	if (tds_thread_create_detached(login_proc, ev) != 0) {
		pool_free_user(pool, puser);
		fprintf(stderr, "error creating thread\n");
		return NULL;
	}

	return puser;
}

/* 
 * pool_free_user
 * close out a disconnected user.
 */
void
pool_free_user(TDS_POOL *pool, TDS_POOL_USER * puser)
{
	TDS_POOL_MEMBER *pmbr = puser->assigned_member;
	if (pmbr) {
		assert(pmbr->current_user == puser);
		pool_deassign_member(pool, pmbr);
		pool_reset_member(pool, pmbr);
	}

	tds_free_socket(puser->sock.tds);
	tds_free_login(puser->login);

	/* make sure to decrement the waiters list if he is waiting */
	if (puser->user_state == TDS_SRV_WAIT)
		dlist_user_remove(&pool->waiters, puser);
	else
		dlist_user_remove(&pool->users, puser);
	pool->num_users--;
	free(puser);
}

/* 
 * pool_process_users
 * check the fd_set for user input, allocate a pool member to it, and forward
 * the query to that member.
 */
void
pool_process_users(TDS_POOL * pool, struct pollfd *fds, unsigned num_fds)
{
	TDS_POOL_USER *puser, *next;
	short revents;

	for (next = dlist_user_first(&pool->users); (puser = next) != NULL; ) {

		next = dlist_user_next(&pool->users, puser);

		if (!puser->sock.tds || puser->sock.poll_index >= num_fds)
			continue;	/* dead connection */

		revents = fds[puser->sock.poll_index].revents;

		if (puser->sock.poll_recv && (revents & POLLIN) != 0) {
			assert(puser->user_state == TDS_SRV_QUERY);
			if (!pool_user_read(pool, puser))
				continue;
		}
		if (puser->sock.poll_send && (revents & POLLOUT) != 0) {
			if (!pool_write_data(&puser->assigned_member->sock, &puser->sock))
				pool_free_member(pool, puser->assigned_member);
		}
	}			/* for */
}

/*
 * pool_user_login
 * Reads clients login packet and forges a login acknowledgement sequence 
 */
static bool
pool_user_login(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDSSOCKET *tds;
	TDSLOGIN *login;

	tds = puser->sock.tds;
	while (tds->in_len <= tds->in_pos)
		if (tds_read_packet(tds) < 0)
			return false;

	tdsdump_log(TDS_DBG_NETWORK, "got packet type %d\n", tds->in_flag);
	if (tds->in_flag == TDS71_PRELOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x701;
		tds->out_flag = TDS_REPLY;
		// TODO proper one !!
		// TODO detect TDS version here ??
		tds_put_n(tds,  "\x00\x00\x1a\x00\x06" /* version */
				"\x01\x00\x20\x00\x01" /* encryption */
				"\x02\x00\x21\x00\x01" /* instance ?? */
				"\x03\x00\x22\x00\x00" /* process id ?? */
				"\x04\x00\x22\x00\x01" /* MARS */
				"\xff"
				"\x0a\x00\x06\x40\x00\x00"
				"\x02"
				"\x00"
				""
				"\x00", 0x23);
		tds_flush_packet(tds);

		/* read another packet */
		tds->in_pos = tds->in_len;
		while (tds->in_len <= tds->in_pos)
			if (tds_read_packet(tds) < 0)
				return false;
	}

	puser->login = login = tds_alloc_login(true);
	if (tds->in_flag == TDS_LOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x500;
		tds_read_login(tds, login);
	} else if (tds->in_flag == TDS7_LOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x700;
		if (!tds7_read_login(tds, login))
			return false;
	} else {
		return false;
	}

	/* check we support version required */
	// TODO function to check it
	if (!IS_TDS71_PLUS(login))
		return false;

	tds->in_len = tds->in_pos = 0;

	dump_login(login);
	if (strcmp(tds_dstr_cstr(&login->user_name), pool->user) != 0
	    || strcmp(tds_dstr_cstr(&login->password), pool->password) != 0)
		/* TODO send nack before exiting */
		return false;

	return true;
}

bool
pool_user_send_login_ack(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	char msg[256];
	char block[32];
	TDSSOCKET *tds = puser->sock.tds, *mtds = puser->assigned_member->sock.tds;
	TDSLOGIN *login = puser->login;
	const char *database;
	const char *server = mtds->conn->server ? mtds->conn->server : "JDBC";
	bool dbname_mismatch, odbc_mismatch;

	pool->user_logins++;

	/* copy a bit of information, resize socket with block */
	tds->conn->tds_version = mtds->conn->tds_version;
	tds->conn->product_version = mtds->conn->product_version;
	memcpy(tds->conn->collation, mtds->conn->collation, sizeof(tds->conn->collation));
	tds->conn->tds71rev1 = mtds->conn->tds71rev1;
	free(tds->conn->product_name);
	tds->conn->product_name = strdup(mtds->conn->product_name);
	tds_realloc_socket(tds, mtds->conn->env.block_size);
	tds->conn->env.block_size = mtds->conn->env.block_size;
	tds->conn->client_spid = mtds->conn->spid;

	/* if database is different use USE statement */
	database = pool->database;
	dbname_mismatch = !tds_dstr_isempty(&login->database)
			  && strcasecmp(tds_dstr_cstr(&login->database), database) != 0;
	odbc_mismatch = (login->option_flag2 & TDS_ODBC_ON) == 0;
	if (dbname_mismatch || odbc_mismatch) {
		char *str;
		int len = 128 + tds_quote_id(mtds, NULL, tds_dstr_cstr(&login->database),-1);
		TDSRET ret;

		if ((str = tds_new(char, len)) == NULL)
			return false;

		str[0] = 0;
		/* swicth to dblib options */
		if (odbc_mismatch)
			strcat(str, "SET ANSI_DEFAULTS OFF\nSET CONCAT_NULL_YIELDS_NULL OFF\n");
		if (dbname_mismatch) {
			strcat(str, "USE ");
			tds_quote_id(mtds, strchr(str, 0), tds_dstr_cstr(&login->database), -1);
		}
		ret = tds_submit_query(mtds, str);
		free(str);
		if (TDS_FAILED(ret) || TDS_FAILED(tds_process_simple_query(mtds)))
			return false;
		if (dbname_mismatch)
			database = tds_dstr_cstr(&login->database);
		else
			database = mtds->conn->env.database;
	}

	// 7.0
	// env database
	// database change message (with server name correct)
	// env language
	// language change message
	// env 0x3 charset ("iso_1")
	// env 0x5 lcid ("1033")
	// env 0x6 ("196609" ?? 0x30001)
	// loginack
	// env 0x4 packet size
	// done
	//
	// 7.1/7.2/7.3
	// env database
	// database change message (with server name correct)
	// env 0x7 collation
	// env language
	// language change message
	// loginack
	// env 0x4 packet size
	// done
	tds->out_flag = TDS_REPLY;
	tds_env_change(tds, TDS_ENV_DATABASE, "master", database);
	sprintf(msg, "Changed database context to '%s'.", database);
	tds_send_msg(tds, 5701, 2, 0, msg, server, NULL, 1);
	if (!login->suppress_language) {
		tds_env_change(tds, TDS_ENV_LANG, NULL, "us_english");
		tds_send_msg(tds, 5703, 1, 0, "Changed language setting to 'us_english'.", server, NULL, 1);
	}

	if (IS_TDS71_PLUS(tds->conn)) {
		tds_put_byte(tds, TDS_ENVCHANGE_TOKEN);
		tds_put_smallint(tds, 8);
		tds_put_byte(tds, TDS_ENV_SQLCOLLATION);
		tds_put_byte(tds, 5);
		tds_put_n(tds, tds->conn->collation, 5);
		tds_put_byte(tds, 0);
	}

	tds_send_login_ack(tds, mtds->conn->product_name);
	sprintf(block, "%d", tds->conn->env.block_size);
	tds_env_change(tds, TDS_ENV_PACKSIZE, block, block);
	/* tds_send_capabilities_token(tds); */
	tds_send_done_token(tds, 0, 0);

	/* send it! */
	tds_flush_packet(tds);

	tds_free_login(login);
	puser->login = NULL;
	return true;
}

/*
 * pool_user_read
 * checks the packet type of data coming from the client and allocates a 
 * pool member if necessary.
 */
static bool
pool_user_read(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDSSOCKET *tds = puser->sock.tds;
	TDS_POOL_MEMBER *pmbr = NULL;

	for (;;) {
		TDS_UCHAR in_flag;

		if (pool_packet_read(tds))
			break;
		if (tds->in_len == 0) {
			tdsdump_log(TDS_DBG_INFO1, "user disconnected\n");
			pool_free_user(pool, puser);
			return false;
		}

		tdsdump_dump_buf(TDS_DBG_NETWORK, "Got packet from client:", tds->in_buf, tds->in_len);

		in_flag = tds->in_buf[0];
		switch (in_flag) {
		case TDS_QUERY:
		case TDS_NORMAL:
		case TDS_RPC:
		case TDS_BULK:
		case TDS_CANCEL:
		case TDS7_TRANS:
			if (!pool_write_data(&puser->sock, &puser->assigned_member->sock)) {
				pool_reset_member(pool, puser->assigned_member);
				return false;
			}
			pmbr = puser->assigned_member;
			break;

		default:
			tdsdump_log(TDS_DBG_ERROR, "Unrecognized packet type, closing user\n");
			pool_free_user(pool, puser);
			return false;
		}
		if (tds->in_pos < tds->in_len)
			/* partial write, schedule a future write */
			break;
	}
	if (pmbr && !pmbr->sock.poll_send)
		tds_socket_flush(tds_get_s(pmbr->sock.tds));
	return true;
}

void
pool_user_query(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDS_POOL_MEMBER *pmbr;

	tdsdump_log(TDS_DBG_FUNC, "pool_user_query\n");

	assert(puser->assigned_member == NULL);
	assert(puser->login);

	puser->user_state = TDS_SRV_QUERY;
	pmbr = pool_assign_idle_member(pool, puser);
	if (!pmbr) {
		/*
		 * put into wait state
		 * check when member is deallocated
		 */
		tdsdump_log(TDS_DBG_INFO1, "Not enough free members...placing user in WAIT\n");
		puser->user_state = TDS_SRV_WAIT;
		puser->sock.poll_recv = false;
		puser->sock.poll_send = false;
		dlist_user_remove(&pool->users, puser);
		dlist_user_append(&pool->waiters, puser);
	}
}

typedef struct {
	TDS_POOL_EVENT common;
	TDS_POOL *pool;
	TDS_POOL_USER *puser;
	bool success;
} END_LOGIN_EVENT;

static TDS_THREAD_PROC_DECLARE(end_login_proc, arg)
{
	END_LOGIN_EVENT *ev = (END_LOGIN_EVENT *) arg;
	TDS_POOL *pool = ev->pool;

	ev->success = pool_user_send_login_ack(pool, ev->puser);

	pool_event_add(pool, &ev->common, end_login_execute);
	return TDS_THREAD_RESULT(0);
}

static void
end_login_execute(TDS_POOL_EVENT *base_event)
{
	END_LOGIN_EVENT *ev = (END_LOGIN_EVENT *) base_event;
	TDS_POOL *pool = ev->pool;
	TDS_POOL_USER *puser = ev->puser;
	TDS_POOL_MEMBER *pmbr = puser->assigned_member;

	if (!ev->success) {
		pool_free_member(pool, pmbr);
		return;
	}

	puser->sock.poll_recv = true;
	puser->sock.poll_send = false;
	pmbr->sock.poll_recv = true;
	pmbr->sock.poll_send = false;
}

/**
 * Handle async login
 */
void
pool_user_finish_login(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	END_LOGIN_EVENT *ev = tds_new0(END_LOGIN_EVENT, 1);
	if (!ev) {
		pool_free_member(pool, puser->assigned_member);
		return;
	}

	ev->pool  = pool;
	ev->puser = puser;

	if (tds_thread_create_detached(end_login_proc, ev) != 0) {
		pool_free_member(pool, puser->assigned_member);
		free(ev);
		fprintf(stderr, "error creating thread\n");
	}
}
