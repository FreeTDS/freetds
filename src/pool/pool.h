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

#ifndef _pool_h_
#define _pool_h_

static char rcsid_pool_h [ ] =
         "$Header: /tmp/gitout/git/../freetds/freetds/src/pool/pool.h,v 1.4 2002-09-20 20:50:58 castellano Exp $";
static void *no_unused_var_warn_pool_h[] = {rcsid_pool_h,
					    no_unused_var_warn_pool_h};

/* includes */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "tds.h"

/* defines */
#define PGSIZ 2048
#define BLOCKSIZ 512
#define MAX_POOL_USERS 1024

/* enums and typedefs */
enum {
	TDS_SRV_LOGIN,
	TDS_SRV_IDLE,
	TDS_SRV_QUERY,
	TDS_SRV_WAIT,    /* if no members are free wait */
	TDS_SRV_CANCEL,
	TDS_SRV_DEAD
};

typedef struct tds_pool_user {
	TDSSOCKET *tds;
	int user_state;
} TDS_POOL_USER;

typedef struct tds_pool_member {
	TDSSOCKET *tds;
	/* sometimes we get a partial packet */
	int need_more;
	int state;
	time_t last_used_tm;
	TDS_POOL_USER *current_user;
	/* 
	** these variables are used for tracking the state of the TDS protocol 
	** so we know when to return the state to TDS_COMPLETED.
	*/
	int num_bytes_left;
	unsigned char fragment[PGSIZ];
} TDS_POOL_MEMBER;

typedef struct tds_pool {
	char *name;
	char *user;
	char *password;
	char *server;
	char *database;
	int port;
	int max_member_age; /* in seconds */
	int min_open_conn;
	int max_open_conn;
	int num_members;
	TDS_POOL_MEMBER *members;
	int max_users;
	TDS_POOL_USER *users;
} TDS_POOL;

/* prototypes */
/* main.c */
TDS_POOL *pool_init(char *name);
void pool_main_loop(TDS_POOL *pool);

/* member.c */
int pool_process_members(TDS_POOL *pool, fd_set *fds);
TDSSOCKET *pool_mbr_login(TDS_POOL *pool);
TDS_POOL_MEMBER *pool_find_idle_member(TDS_POOL *pool);
void pool_mbr_init(TDS_POOL *pool);

/* user.c */
int pool_process_users(TDS_POOL *pool, fd_set *fds);
void pool_user_init(TDS_POOL *pool);
TDS_POOL_USER *pool_user_create(TDS_POOL *pool, int s, struct sockaddr_in *sin);
void pool_free_user(TDS_POOL_USER *puser);
void pool_user_read(TDS_POOL *pool, TDS_POOL_USER *puser);
int pool_user_login(TDS_POOL *pool, TDS_POOL_USER *puser);
void pool_user_query(TDS_POOL *pool, TDS_POOL_USER *puser);

/* util.c */
void dump_buf(const void *buf,int length);
void dump_login(TDSLOGIN *login);
void die_if(int expr, const char *msg);

/* stream.c */
int pool_find_end_token(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int len);

/* config.c */
int pool_read_conf_file(char *poolname, TDS_POOL *pool);


#endif

