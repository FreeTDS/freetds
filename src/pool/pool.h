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

#ifndef _tdsguard_clBmMDiJ1W6vO4Q4ftyzgV_
#define _tdsguard_clBmMDiJ1W6vO4Q4ftyzgV_

#include <assert.h>

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
#include <freetds/utils/dlist.h>
#include <freetds/replacements.h>

/* defines */
#define PGSIZ 2048
#define BLOCKSIZ 512
#define MAX_POOL_USERS 1024

/* enums and typedefs */
typedef enum
{
	TDS_SRV_WAIT,		/* if no members are free wait */
	TDS_SRV_QUERY,
} TDS_USER_STATE;

/* forward declaration */
typedef struct tds_pool_event TDS_POOL_EVENT;
typedef struct tds_pool_socket TDS_POOL_SOCKET;
typedef struct tds_pool_member TDS_POOL_MEMBER;
typedef struct tds_pool_user TDS_POOL_USER;
typedef struct tds_pool TDS_POOL;
typedef void (*TDS_POOL_EXECUTE)(TDS_POOL_EVENT *event);

struct tds_pool_event
{
	TDS_POOL_EVENT *next;
	TDS_POOL_EXECUTE execute;
};

struct tds_pool_socket
{
	TDSSOCKET *tds;
	uint32_t poll_index;
	bool poll_recv;
	bool poll_send;
};

struct tds_pool_user
{
	TDS_POOL_SOCKET sock;
	DLIST_FIELDS(dlist_user_item);
	TDSLOGIN *login;
	TDS_USER_STATE user_state;
	TDS_POOL_MEMBER *assigned_member;
};

struct tds_pool_member
{
	TDS_POOL_SOCKET sock;
	DLIST_FIELDS(dlist_member_item);
	bool doing_async;
	time_t last_used_tm;
	TDS_POOL_USER *current_user;
};

#define DLIST_PREFIX dlist_member
#define DLIST_LIST_TYPE dlist_members
#define DLIST_ITEM_TYPE TDS_POOL_MEMBER
#include <freetds/utils/dlist.tmpl.h>

#define DLIST_PREFIX dlist_user
#define DLIST_LIST_TYPE dlist_users
#define DLIST_ITEM_TYPE TDS_POOL_USER
#include <freetds/utils/dlist.tmpl.h>

struct tds_pool
{
	char *name;
	char *user;
	char *password;
	char *server;
	char *database;
	char *server_user;
	char *server_password;
	int port;
	int max_member_age;	/* in seconds */
	int min_open_conn;
	int max_open_conn;
	tds_mutex events_mtx;
	TDS_SYS_SOCKET listen_fd;
	TDS_SYS_SOCKET wakeup_fd;
	TDS_SYS_SOCKET event_fd;
	TDS_POOL_EVENT *events;

	int num_active_members;
	dlist_members active_members;
	dlist_members idle_members;

	/** users in wait state */
	dlist_users waiters;
	int num_users;
	dlist_users users;
	TDSCONTEXT *ctx;

	unsigned long user_logins;
	unsigned long member_logins;
};

/* prototypes */

/* member.c */
int pool_process_members(TDS_POOL * pool, struct pollfd *fds, unsigned num_fds);
TDS_POOL_MEMBER *pool_assign_idle_member(TDS_POOL * pool, TDS_POOL_USER *user);
void pool_mbr_init(TDS_POOL * pool);
void pool_mbr_destroy(TDS_POOL * pool);
void pool_free_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr);
void pool_assign_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr, TDS_POOL_USER *puser);
void pool_deassign_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr);
void pool_reset_member(TDS_POOL *pool, TDS_POOL_MEMBER * pmbr);
bool pool_packet_read(TDSSOCKET * tds);
#if ENABLE_EXTRA_CHECKS
void pool_mbr_check(TDS_POOL *pool);
#else
static inline void pool_mbr_check(TDS_POOL *pool TDS_UNUSED)
{
}
#endif


/* user.c */
void pool_process_users(TDS_POOL * pool, struct pollfd *fds, unsigned num_fds);
void pool_user_init(TDS_POOL * pool);
void pool_user_destroy(TDS_POOL * pool);
TDS_POOL_USER *pool_user_create(TDS_POOL * pool, TDS_SYS_SOCKET s);
void pool_free_user(TDS_POOL * pool, TDS_POOL_USER * puser);
void pool_user_query(TDS_POOL * pool, TDS_POOL_USER * puser);
bool pool_user_send_login_ack(TDS_POOL * pool, TDS_POOL_USER * puser);
void pool_user_finish_login(TDS_POOL * pool, TDS_POOL_USER * puser);

/* util.c */
void dump_login(TDSLOGIN * login);
void pool_event_add(TDS_POOL *pool, TDS_POOL_EVENT *ev, TDS_POOL_EXECUTE execute);
int pool_write(TDS_SYS_SOCKET sock, const void *buf, size_t len);
bool pool_write_data(TDS_POOL_SOCKET *from, TDS_POOL_SOCKET *to);

/* config.c */
bool pool_read_conf_files(const tds_dir_char *path, const char *poolname, TDS_POOL * pool, char **err);


#endif
