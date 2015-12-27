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

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

/* 
 * POSIX says fd_set type may be defined in either sys/select.h or sys/time.h. 
 */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <freetds/tds.h>

/* defines */
#define PGSIZ 2048
#define BLOCKSIZ 512
#define MAX_POOL_USERS 1024

/* enums and typedefs */
typedef enum
{
	TDS_SRV_LOGIN,
	TDS_SRV_WAIT,		/* if no members are free wait */
	TDS_SRV_QUERY,
} TDS_USER_STATE;

/* forward declaration */
typedef struct tds_pool_member TDS_POOL_MEMBER;
typedef struct tds_pool_user TDS_POOL_USER;
typedef struct tds_pool TDS_POOL;

struct tds_pool_user
{
	TDSSOCKET *tds;
	TDSLOGIN *login;
	TDS_USER_STATE user_state;
	bool poll_recv;
	TDS_POOL_MEMBER *assigned_member;
};

struct tds_pool_member
{
	TDSSOCKET *tds;
	time_t last_used_tm;
	TDS_POOL_USER *current_user;
};

struct tds_pool
{
	char *name;
	char *user;
	char *password;
	char *server;
	char *database;
	int port;
	int max_member_age;	/* in seconds */
	int min_open_conn;
	int max_open_conn;
	int num_members;
	int active_members;
	TDS_POOL_MEMBER *members;
	/** number of users in wait state */
	int waiters;
	int max_users;
	TDS_POOL_USER *users;
	TDSCONTEXT *ctx;
};

/* prototypes */

/* member.c */
int pool_process_members(TDS_POOL * pool, fd_set * fds);
TDS_POOL_MEMBER *pool_find_idle_member(TDS_POOL * pool, TDS_POOL_USER *user);
void pool_mbr_init(TDS_POOL * pool);
void pool_mbr_destroy(TDS_POOL * pool);
void pool_free_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr);
void pool_assign_member(TDS_POOL_MEMBER * pmbr, TDS_POOL_USER *puser);
void pool_deassign_member(TDS_POOL_MEMBER * pmbr);
void pool_reset_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr);
bool pool_packet_read(TDSSOCKET * tds);

/* user.c */
int pool_process_users(TDS_POOL * pool, fd_set * fds);
void pool_user_init(TDS_POOL * pool);
void pool_user_destroy(TDS_POOL * pool);
TDS_POOL_USER *pool_user_create(TDS_POOL * pool, TDS_SYS_SOCKET s);
void pool_free_user(TDS_POOL * pool, TDS_POOL_USER * puser);
void pool_user_query(TDS_POOL * pool, TDS_POOL_USER * puser);

/* util.c */
void dump_login(TDSLOGIN * login);
void die_if(int expr, const char *msg);
int pool_write_all(TDS_SYS_SOCKET s, const void *buf, size_t len);

/* config.c */
int pool_read_conf_file(const char *poolname, TDS_POOL * pool);


#endif
