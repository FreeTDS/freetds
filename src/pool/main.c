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
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

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
static int got_sigterm = 0;
static int got_sighup = 0;
static const char *logfile_name = NULL;

static void sigterm_handler(int sig);
static void pool_schedule_waiters(TDS_POOL * pool);
static TDS_POOL *pool_init(const char *name);
static void pool_socket_init(TDS_POOL * pool);
static void pool_main_loop(TDS_POOL * pool);
static bool pool_open_logfile(TDS_POOL * pool);

static void
sigterm_handler(int sig)
{
	got_sigterm = 1;
}

static void
sighup_handler(int sig)
{
	got_sighup = 1;
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

	pool = tds_new0(TDS_POOL, 1);
	pool->password = strdup("");

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
	check_field(name, pool->user != NULL,   "user");
	check_field(name, pool->server != NULL, "server");
	check_field(name, pool->port != 0,   "port");

	if (pool->max_open_conn < pool->min_open_conn) {
		fprintf(stderr, "Max connections less than minimum\n");
		exit(EXIT_FAILURE);
	}

	pool->name = strdup(name);

	pool_open_logfile(pool);

	pool_mbr_init(pool);
	pool_user_init(pool);

	pool_socket_init(pool);

	return pool;
}

static void
pool_destroy(TDS_POOL *pool)
{
	pool_mbr_destroy(pool);
	pool_user_destroy(pool);

	CLOSESOCKET(pool->wakeup_fd);
	CLOSESOCKET(pool->listen_fd);
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

	/* first see if there are free members to do the request */
	if (!dlist_member_first(&pool->idle_members))
		return;

	while ((puser = dlist_user_first(&pool->waiters)) != NULL) {
		if (puser->user_state == TDS_SRV_WAIT) {
			/* place back in query state */
			puser->user_state = TDS_SRV_QUERY;
			dlist_user_remove(&pool->waiters, puser);
			dlist_user_append(&pool->users, puser);
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

static bool
pool_open_logfile(TDS_POOL *pool)
{
	int fd;

	tds_g_append_mode = 0;
	tdsdump_open(getenv("TDSDUMP"));

	if (!logfile_name)
		return true;
	fd = open(logfile_name, O_WRONLY|O_CREAT|O_APPEND, 0644);
	if (fd < 0)
		return false;

	fflush(stdout);
	fflush(stderr);
	while (dup2(fd, fileno(stdout)) < 0 && errno == EINTR)
		continue;
	while (dup2(fd, fileno(stderr)) < 0 && errno == EINTR)
		continue;
	close(fd);
	fflush(stdout);
	fflush(stderr);

	return true;
}

static void
pool_socket_init(TDS_POOL * pool)
{
	struct sockaddr_in sin;
	TDS_SYS_SOCKET s, event_pair[2];
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
	pool->listen_fd = s;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, event_pair) < 0) {
		perror("socketpair");
		exit(1);
	}
	tds_socket_set_nonblocking(event_pair[0]);
	tds_socket_set_nonblocking(event_pair[1]);
	pool->event_fd = event_pair[1];
	pool->wakeup_fd = event_pair[0];
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
	TDS_POOL_USER *puser;
	TDS_SYS_SOCKET s, wakeup;
	SELECT_INFO sel;

	s = pool->listen_fd;
	wakeup = pool->wakeup_fd;

	while (!got_sigterm) {

		FD_ZERO(&sel.rfds);
		FD_ZERO(&sel.wfds);
		/* add the listening socket to the read list */
		FD_SET(s, &sel.rfds);
		FD_SET(wakeup, &sel.rfds);
		sel.maxfd = s > wakeup ? s : wakeup;

		/* add the user sockets to the read list */
		DLIST_FOREACH(dlist_user, &pool->users, puser)
			pool_select_add_socket(&sel, &puser->sock);

		/* add the pool member sockets to the read list */
		DLIST_FOREACH(dlist_member, &pool->active_members, pmbr)
			pool_select_add_socket(&sel, &pmbr->sock);

		/* FIXME check return value */
		select(sel.maxfd + 1, &sel.rfds, &sel.wfds, NULL, NULL);
		if (TDS_UNLIKELY(got_sigterm))
			break;

		if (TDS_UNLIKELY(got_sighup)) {
			got_sighup = 0;
			pool_open_logfile(pool);
		}

		/* process events */
		if (FD_ISSET(wakeup, &sel.rfds)) {
			char buf[32];
			READSOCKET(wakeup, buf, sizeof(buf));

			pool_process_events(pool);
		}

		/* process the sockets */
		if (FD_ISSET(s, &sel.rfds)) {
			pool_user_create(pool, s);
		}
		pool_process_users(pool, &sel.rfds, &sel.wfds);
		pool_process_members(pool, &sel.rfds, &sel.wfds);

		/* back from members */
		if (dlist_user_first(&pool->waiters))
			pool_schedule_waiters(pool);
	}			/* while !got_sigterm */
	tdsdump_log(TDS_DBG_INFO2, "Shutdown Requested\n");
}

static void
print_usage(const char *progname)
{
	fprintf(stderr, "Usage:\t%s [-l <log file>] [-d] <pool name>\n", progname);
}

int
main(int argc, char **argv)
{
	int opt;
#ifdef HAVE_FORK
	bool daemonize = false;
#  define DAEMON_OPT "d"
#else
#  define DAEMON_OPT ""
#endif
	TDS_POOL *pool;

	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigterm_handler);
#ifndef _WIN32
	signal(SIGHUP, sighup_handler);
	signal(SIGPIPE, SIG_IGN);
#endif

	while ((opt = getopt(argc, argv, "l:" DAEMON_OPT)) != -1) {
		switch (opt) {
		case 'l':
			logfile_name = optarg;
			break;
#ifdef HAVE_FORK
		case 'd':
			daemonize = true;
			break;
#endif
		default:
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}
	if (optind >= argc) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	pool = pool_init(argv[optind]);
#ifdef HAVE_FORK
	if (daemonize) {
		if (daemon(0, 0) < 0) {
			fprintf(stderr, "Failed to daemonize %s\n", argv[0]);
			return EXIT_FAILURE;
		}
	}
#endif
	pool_main_loop(pool);
	printf("User logins %lu members logins %lu members at end %d\n", pool->user_logins, pool->member_logins, pool->num_active_members);
	pool_destroy(pool);
	printf("tdspool Shutdown\n");
	return EXIT_SUCCESS;
}
