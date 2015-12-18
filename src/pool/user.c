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

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#include "pool.h"
#include <freetds/server.h>
#include <freetds/string.h>

static TDS_POOL_USER *pool_user_find_new(TDS_POOL * pool);
static int pool_user_login(TDS_POOL * pool, TDS_POOL_USER * puser);
static void pool_user_read(TDS_POOL * pool, TDS_POOL_USER * puser);
static void pool_user_write(TDS_POOL * pool, TDS_POOL_USER * puser);

extern int waiters;

void
pool_user_init(TDS_POOL * pool)
{
	/* allocate room for pool users */

	pool->users = (TDS_POOL_USER *)
		calloc(MAX_POOL_USERS, sizeof(TDS_POOL_USER));
	pool->ctx = tds_alloc_context(NULL);
}

void
pool_user_destroy(TDS_POOL * pool)
{
	TDS_POOL_USER *puser;
	int i;

	for (i = 0; i < pool->max_users; i++) {
		puser = &pool->users[i];
		if (!IS_TDSDEAD(puser->tds)) {
			fprintf(stderr, "Closing user %d\n", i);
			tds_close_socket(puser->tds);
		}
		if (puser->tds) {
			tds_free_socket(puser->tds);
			puser->tds = NULL;
		}
	}

	free(pool->users);
	pool->users = NULL;
	tds_free_context(pool->ctx);
	pool->ctx = NULL;
}

static TDS_POOL_USER *
pool_user_find_new(TDS_POOL * pool)
{
	TDS_POOL_USER *puser;
	int i;

	/* first check for dead users to reuse */
	for (i=0; i<pool->max_users; i++) {
		puser = &pool->users[i];
		if (!puser->tds) {
			puser->poll_recv = true;
			return puser;
		}
	}

	/* did we exhaust the number of concurrent users? */
	if (pool->max_users >= MAX_POOL_USERS) {
		fprintf(stderr, "Max concurrent users exceeded, increase in pool.h\n");
		return NULL;
	}

	/* else take one off the top of the pool->users */
	puser = &pool->users[pool->max_users];
	pool->max_users++;

	puser->poll_recv = true;
	return puser;
}

/*
 * pool_user_create
 * accepts a client connection and adds it to the users list and returns it
 */
TDS_POOL_USER *
pool_user_create(TDS_POOL * pool, TDS_SYS_SOCKET s, struct sockaddr_in *sin)
{
	TDS_POOL_USER *puser;
	TDS_SYS_SOCKET fd;
	socklen_t len;
	TDSSOCKET *tds;

	fprintf(stderr, "accepting connection\n");
	len = sizeof(*sin);
	if (TDS_IS_SOCKET_INVALID(fd = tds_accept(s, (struct sockaddr *) sin, &len))) {
		perror("accept");
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
	if (TDS_FAILED(tds_iconv_open(tds->conn, "UTF-8", 0))) {
		tds_free_socket(tds);
		CLOSESOCKET(fd);
		return NULL;
	}
	tds_set_parent(tds, NULL);
	/* FIX ME - little endian emulation should be config file driven */
	tds->conn->emul_little_endian = 1;
	tds_set_s(tds, fd);
	tds->state = TDS_IDLE;
	tds->out_flag = TDS_LOGIN;
	puser->tds = tds;
	puser->user_state = TDS_SRV_LOGIN;
	return puser;
}

/* 
 * pool_free_user
 * close out a disconnected user.
 */
void
pool_free_user(TDS_POOL_USER * puser)
{
	if (puser->assigned_member) {
		pool_deassign_member(puser->assigned_member);
		puser->assigned_member = NULL;
	}

	/* make sure to decrement the waiters list if he is waiting */
	if (puser->user_state == TDS_SRV_WAIT)
		waiters--;
	tds_free_socket(puser->tds);
	tds_free_login(puser->login);
	memset(puser, 0, sizeof(TDS_POOL_USER));
}

/* 
 * pool_process_users
 * check the fd_set for user input, allocate a pool member to it, and forward
 * the query to that member.
 */
int
pool_process_users(TDS_POOL * pool, fd_set * fds)
{
	TDS_POOL_USER *puser;
	int i;
	int cnt = 0;

	for (i = 0; i < pool->max_users; i++) {

		puser = &pool->users[i];

		if (!puser->tds)
			continue;	/* dead connection */

		if (FD_ISSET(tds_get_s(puser->tds), fds)) {
			cnt++;
			switch (puser->user_state) {
			case TDS_SRV_LOGIN:
				if (pool_user_login(pool, puser)) {
					/* login failed...free socket */
					pool_free_user(puser);
				}
				/* otherwise we have a good login */
				break;
			case TDS_SRV_QUERY:
				/* what is this? a cancel perhaps */
				pool_user_read(pool, puser);
				break;
			/* just to avoid a warning */
			case TDS_SRV_WAIT:
				break;
			}	/* switch */
		}		/* if */
	}			/* for */
	return cnt;
}

/*
 * pool_user_login
 * Reads clients login packet and forges a login acknowledgement sequence 
 */
static int
pool_user_login(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDSSOCKET *tds;
	TDSLOGIN *login;

	tds = puser->tds;
	while (tds->in_len <= tds->in_pos)
		if (tds_read_packet(tds) < 0)
			return 1;

	tdsdump_log(TDS_DBG_NETWORK, "got packet type %d\n", tds->in_flag);
	if (tds->in_flag == TDS71_PRELOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x701;
		tds->out_flag = TDS_REPLY;
		// TODO proper one !!
		// TODO detect TDS version here ??
		tds_put_n(tds, "\x00\x00\x1a\x00\x06\x01\x00\x20\x00\x01\x02\x00\x21\x00\x01\x03\x00\x22\x00\x00\x04\x00\x22\x00\x01\xff\x0a\x00\x06\x40\x00\x00\x02\x01\x00", 0x23);
		tds_flush_packet(tds);
		tds->in_pos = tds->in_len;
		return 0;
	}

	login = tds_alloc_login(1);
	if (tds->in_flag == TDS_LOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x500;
		tds_read_login(tds, login);
	} else if (tds->in_flag == TDS7_LOGIN) {
		if (!tds->conn->tds_version)
			tds->conn->tds_version = 0x700;
		if (!tds7_read_login(tds, login)) {
			tds_free_login(login);
			return 1;
		}
	} else {
		tds_free_login(login);
		return 1;
	}

	/* check we support version required */
	if (!IS_TDS71_PLUS(login)) {
		tds_free_login(login);
		return 1;
	}

	tds->in_len = tds->in_pos = 0;

	dump_login(login);
	if (!strcmp(tds_dstr_cstr(&login->user_name), pool->user) && !strcmp(tds_dstr_cstr(&login->password), pool->password)) {
		puser->login = login;

		/* try to assign a member, connection can have transactions
		 * and so on so deassign only when disconnected */
		pool_user_query(pool, puser);

		tdsdump_log(TDS_DBG_INFO1, "user state %d\n", puser->user_state);

		assert(puser->login || puser->user_state == TDS_SRV_QUERY);

		return 0;
	}

	tds_free_login(login);
	/* TODO send nack before exiting */
	return 1;
}

static void
pool_user_send_login_ack(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	char msg[256];
	char block[32];
	TDSSOCKET *tds = puser->tds, *mtds = puser->assigned_member->tds;
	TDSLOGIN *login = puser->login;
	const char *database = pool->database;

	/* copy a bit of information, resize socket with block */
	tds->conn->tds_version = mtds->conn->tds_version;
	tds->conn->product_version = mtds->conn->product_version;
	memcpy(tds->conn->collation, mtds->conn->collation, sizeof(tds->conn->collation));
	tds->conn->tds71rev1 = mtds->conn->tds71rev1;
	free(tds->conn->product_name);
	tds->conn->product_name = strdup(mtds->conn->product_name);
	tds_realloc_socket(tds, mtds->conn->env.block_size);
	tds->conn->env.block_size = mtds->conn->env.block_size;

	if (!database)
		database = mtds->conn->env.database;

	tds->out_flag = TDS_REPLY;
	tds_env_change(tds, TDS_ENV_DATABASE, "master", database);
	sprintf(msg, "Changed database context to '%s'.", database);
	tds_send_msg(tds, 5701, 2, 10, msg, "JDBC", "ZZZZZ", 1);
	if (!login->suppress_language) {
		tds_env_change(tds, TDS_ENV_LANG, NULL, "us_english");
		tds_send_msg(tds, 5703, 1, 10, "Changed language setting to 'us_english'.", "JDBC", "ZZZZZ", 1);
	}

	if (IS_TDS7_PLUS(tds->conn)) {
		tds_put_byte(tds, TDS_ENVCHANGE_TOKEN);
		tds_put_smallint(tds, 8);
		tds_put_byte(tds, TDS_ENV_SQLCOLLATION);
		tds_put_byte(tds, 5);
		tds_put_n(tds, tds->conn->collation, 5);
		tds_put_byte(tds, 0);
	}

	sprintf(block, "%d", tds->conn->env.block_size);
	tds_env_change(tds, TDS_ENV_PACKSIZE, block, block);
	tds_send_login_ack(tds, mtds->conn->product_name);
	/* tds_send_capabilities_token(tds); */
	tds_send_done_token(tds, 0, 0);

	/* send it! */
	tds_flush_packet(tds);
	tds_free_login(login);
	puser->login = NULL;
}

/*
 * pool_user_read
 * checks the packet type of data coming from the client and allocates a 
 * pool member if necessary.
 */
static void
pool_user_read(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDSSOCKET *tds = puser->tds;
	TDS_POOL_MEMBER *pmbr = NULL;

	for (;;) {
		if (pool_packet_read(tds))
			break;
		if (tds->in_len <= 0) {
			if (tds->in_len == 0) {
				fprintf(stderr, "user disconnected\n");
			} else {
				perror("read");
				fprintf(stderr, "cleaning up user\n");
			}

			pmbr = puser->assigned_member;
			if (pmbr) {
				fprintf(stderr, "user has assigned member, freeing\n");
				pool_reset_member(pool, pmbr);
			} else {
				pool_free_user(puser);
			}
			return;
		} else {
			TDS_UCHAR in_flag = tds->in_buf[0];

			tdsdump_dump_buf(TDS_DBG_NETWORK, "Got packet from client:", tds->in_buf, tds->in_len);

			switch (in_flag) {
			case TDS_QUERY:
			case TDS_NORMAL:
			case TDS_RPC:
			case TDS_BULK:
			case TDS_CANCEL:
			case TDS7_TRANS:
				pool_user_write(pool, puser);
				pmbr = puser->assigned_member;
				break;

			default:
				fprintf(stderr, "Unrecognized packet type, closing user\n");
				pool_free_user(puser);
				return;
			}
		}
	}
	if (pmbr)
		tds_socket_flush(tds_get_s(pmbr->tds));
}

void
pool_user_query(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDS_POOL_MEMBER *pmbr;

	tdsdump_log(TDS_DBG_FUNC, "pool_user_query\n");

	assert(puser->assigned_member == NULL);
	assert(puser->login);

	pmbr = pool_find_idle_member(pool, puser);
	if (!pmbr) {
		/*
		 * put into wait state
		 * check when member is deallocated
		 */
		fprintf(stderr, "Not enough free members...placing user in WAIT\n");
		puser->user_state = TDS_SRV_WAIT;
		puser->poll_recv = false;
		waiters++;
		return;
	}

	puser->poll_recv = true;
	pool_assign_member(pmbr, puser);
	pool_user_send_login_ack(pool, puser);
	puser->user_state = TDS_SRV_QUERY;
}

static void
pool_user_write(TDS_POOL * pool, TDS_POOL_USER * puser)
{
	TDS_POOL_MEMBER *pmbr = puser->assigned_member;
	int ret;
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_INFO1, "trying to send\n");

	tds = puser->tds;
	tdsdump_log(TDS_DBG_INFO1, "sending %d bytes\n", tds->in_len);
	pmbr->state = TDS_WRITING;
	/* cf. net.c for better technique.  */
	ret = pool_write_all(tds_get_s(pmbr->tds), tds->in_buf + tds->in_pos, tds->in_len - tds->in_pos);
	/* write failed, cleanup member */
	if (ret <= 0) {
		pool_free_member(pool, pmbr);
	}
	tds->in_pos = 0;
}
