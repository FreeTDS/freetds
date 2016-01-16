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

/*
 * Note on terminology: a pool member is a connection to the database,
 * a pool user is a client connection that is temporarily assigned to a
 * pool member.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

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

/* to be set by sig term */
static int term = 0;

static void term_handler(int sig);
static void pool_schedule_waiters(TDS_POOL * pool);
static TDS_POOL *pool_init(const char *name);
static void pool_main_loop(TDS_POOL * pool);

static void
term_handler(int sig)
{
	printf("Shutdown Requested\n");
	term = 1;
}

static void
check_field(const char *pool_name, bool cond, const char *field_name)
{
	if (!cond) {
		fprintf(stderr, "No %s specified for pool ``%s''.\n", field_name, pool_name);
		exit(EXIT_FAILURE);
	}
}

/*
 * pool_init creates a named pool and opens connections to the database
 */
static TDS_POOL *
pool_init(const char *name)
{
	TDS_POOL *pool;
	char *err = NULL;

	/* initialize the pool */

	pool = (TDS_POOL *) calloc(1, sizeof(TDS_POOL));

	pool->event_fd = INVALID_SOCKET;
	if (tds_mutex_init(&pool->events_mtx)) {
		fprintf(stderr, "Error initializing pool mutex\n");
		exit(EXIT_FAILURE);
	}

	/* FIXME -- read this from the conf file */
	if (!pool_read_conf_file(name, pool, &err)) {
		fprintf(stderr, "Configuration for pool ``%s'' not found.\n", name);
		exit(EXIT_FAILURE);
	}

	if (err) {
		fprintf(stderr, "%s\n", err);
		exit(EXIT_FAILURE);
	}
	check_field(name, pool->user,   "user");
	check_field(name, pool->server, "server");
	check_field(name, pool->port,   "port");

	if (pool->max_open_conn < pool->min_open_conn) {
		fprintf(stderr, "Max connections less than minimum\n");
		exit(EXIT_FAILURE);
	}

	pool->name = strdup(name);

	pool_mbr_init(pool);
	pool_user_init(pool);

	return pool;
}

static void
pool_destroy(TDS_POOL *pool)
{
	pool_mbr_destroy(pool);
	pool_user_destroy(pool);

	CLOSESOCKET(pool->event_fd);
	tds_mutex_free(&pool->events_mtx);

	free(pool->user);
	free(pool->password);
	free(pool->server);
	free(pool->database);
	free(pool->name);
	free(pool);
}

static void
pool_schedule_waiters(TDS_POOL * pool)
{
	TDS_POOL_USER *puser;
	TDS_POOL_MEMBER *pmbr;
	int i, free_mbrs;

	/* first see if there are free members to do the request */
	free_mbrs = 0;
	DLIST_FOREACH(dlist_member, &pool->active_members, pmbr) {
		if (pmbr->sock.tds)
			free_mbrs++;
	}

	if (!free_mbrs)
		return;

	for (i = 0; i < pool->max_users; i++) {
		puser = &pool->users[i];
		if (puser->user_state == TDS_SRV_WAIT) {
			/* place back in query state */
			puser->user_state = TDS_SRV_QUERY;
			pool->waiters--;
			/* now try again */
			pool_user_query(pool, puser);
			return;
		}
	}
}

typedef struct select_info
{
	fd_set rfds, wfds;
	TDS_SYS_SOCKET maxfd;
} SELECT_INFO;

static void
pool_select_add_socket(SELECT_INFO *sel, TDS_POOL_SOCKET *sock)
{
	/* skip dead connections */
	if (IS_TDSDEAD(sock->tds))
		return;
	if (!sock->poll_recv && !sock->poll_send)
		return;
	if (tds_get_s(sock->tds) > sel->maxfd)
		sel->maxfd = tds_get_s(sock->tds);
	if (sock->poll_recv)
		FD_SET(tds_get_s(sock->tds), &sel->rfds);
	if (sock->poll_send)
		FD_SET(tds_get_s(sock->tds), &sel->wfds);
}

static void
pool_process_events(TDS_POOL *pool)
{
	TDS_POOL_EVENT *events, *next;

	/* detach events from pool */
	tds_mutex_lock(&pool->events_mtx);
	events = pool->events;
	pool->events = NULL;
	tds_mutex_unlock(&pool->events_mtx);

	/* process them */
	while (events) {
		next = events->next;
		events->next = NULL;

		events->execute(events);
		free(events);
		events = next;
	}
}

/* 
 * pool_main_loop
 * Accept new connections from clients, and handle all input from clients and
 * pool members.
 */
static void
pool_main_loop(TDS_POOL * pool)
{
	TDS_POOL_MEMBER *pmbr;
	struct sockaddr_in sin;
	TDS_SYS_SOCKET s, event_pair[2];
	int i;
	SELECT_INFO sel;
	int socktrue = 1;

	/* FIXME -- read the interfaces file and bind accordingly */
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(pool->port);
	sin.sin_family = AF_INET;

	if (TDS_IS_SOCKET_INVALID(s = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("socket");
		exit(1);
	}
	tds_socket_set_nonblocking(s);
	/* don't keep addr in use from s.craig@andronics.com */
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void *) &socktrue, sizeof(socktrue));

	fprintf(stderr, "Listening on port %d\n", pool->port);
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("bind");
		exit(1);
	}
	listen(s, 5);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, event_pair) < 0) {
		perror("socketpair");
		exit(1);
	}
	tds_socket_set_nonblocking(event_pair[0]);
	tds_socket_set_nonblocking(event_pair[1]);
	pool->event_fd = event_pair[1];
	event_pair[1] = INVALID_SOCKET;

	while (!term) {

		FD_ZERO(&sel.rfds);
		FD_ZERO(&sel.wfds);
		/* add the listening socket to the read list */
		FD_SET(s, &sel.rfds);
		FD_SET(event_pair[0], &sel.rfds);
		sel.maxfd = s > event_pair[0] ? s : event_pair[0];

		/* add the user sockets to the read list */
		for (i = 0; i < pool->max_users; i++)
			pool_select_add_socket(&sel, &pool->users[i].sock);

		/* add the pool member sockets to the read list */
		DLIST_FOREACH(dlist_member, &pool->active_members, pmbr)
			pool_select_add_socket(&sel, &pmbr->sock);

		/* fprintf(stderr, "waiting for a connect\n"); */
		/* FIXME check return value */
		select(sel.maxfd + 1, &sel.rfds, &sel.wfds, NULL, NULL);
		if (term)
			break;

		/* process events */
		if (FD_ISSET(event_pair[0], &sel.rfds)) {
			char buf[32];
			READSOCKET(event_pair[0], buf, sizeof(buf));

			pool_process_events(pool);
		}

		/* process the sockets */
		if (FD_ISSET(s, &sel.rfds)) {
			pool_user_create(pool, s);
		}
		pool_process_users(pool, &sel.rfds, &sel.wfds);
		pool_process_members(pool, &sel.rfds, &sel.wfds);

		/* back from members */
		if (pool->waiters) {
			pool_schedule_waiters(pool);
		}
	}			/* while !term */
	CLOSESOCKET(event_pair[0]);
	CLOSESOCKET(s);
}

int
main(int argc, char **argv)
{
	TDS_POOL *pool;

	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	signal(SIGPIPE, SIG_IGN);

	if (argc < 2) {
		fprintf(stderr, "Usage: tdspool <pool name>\n");
		return EXIT_FAILURE;
	}
	pool = pool_init(argv[1]);
	tdsdump_open(getenv("TDSDUMP"));
	pool_main_loop(pool);
	printf("User logins %lu members logins %lu members at end %d\n", pool->user_logins, pool->member_logins, pool->num_active_members);
	pool_destroy(pool);
	printf("tdspool Shutdown\n");
	return EXIT_SUCCESS;
}
