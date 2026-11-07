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
#include <errno.h>
#include <fcntl.h>

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

#ifdef _WIN32
#include <io.h>
#endif

#include "pool.h"

/* to be set by sig term */
static bool got_sigterm = false;
static const char *logfile_name = NULL;

static void sigterm_handler(int sig);
static void pool_schedule_waiters(TDS_POOL * pool);
static void pool_socket_init(TDS_POOL * pool);
static void pool_main_loop(TDS_POOL * pool);
static bool pool_open_logfile(void);

static void
sigterm_handler(int sig TDS_UNUSED)
{
	got_sigterm = true;
}

#ifndef _WIN32
static bool got_sighup = false;

static void
sighup_handler(int sig TDS_UNUSED)
{
	got_sighup = true;
}
#endif

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
pool_init(const char *name, const tds_dir_char *config_path)
{
	TDS_POOL *pool;
	char *err = NULL;

	/* initialize the pool */

	pool = tds_new0(TDS_POOL, 1);
	if (!pool) {
		fprintf(stderr, "Could not allocate memory for pool\n");
		exit(EXIT_FAILURE);
	}
	pool->password = strdup("");

	pool->event_fd = INVALID_SOCKET;
	if (tds_mutex_init(&pool->events_mtx)) {
		fprintf(stderr, "Error initializing pool mutex\n");
		exit(EXIT_FAILURE);
	}

	/* FIXME -- read this from the conf file */
	if (!pool_read_conf_files(config_path, name, pool, &err)) {
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

	if (!pool->server_user)
		pool->server_user = strdup(pool->user);
	if (!pool->server_password)
		pool->server_password = strdup(pool->password);

	if (pool->max_open_conn < pool->min_open_conn) {
		fprintf(stderr, "Max connections less than minimum\n");
		exit(EXIT_FAILURE);
	}

	pool->name = strdup(name);

	pool_open_logfile();

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
	free(pool->server_user);
	free(pool->server_password);
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
	struct pollfd *fds;
	uint32_t num_fds, alloc_fds;
} SELECT_INFO;

static void
pool_select_add_socket(SELECT_INFO *sel, TDS_POOL_SOCKET *sock)
{
	short events;
	struct pollfd *fd;

	/* skip dead connections */
	if (IS_TDSDEAD(sock->tds))
		return;
	if (!sock->poll_recv && !sock->poll_send)
		return;

	events = 0;
	if (sock->poll_recv)
		events |= POLLIN;
	if (sock->poll_send)
		events |= POLLOUT;
	if (sel->num_fds >= sel->alloc_fds) {
		sel->alloc_fds *= 2;
		if (!TDS_RESIZE(sel->fds, sel->alloc_fds)) {
			fprintf(stderr, "Out of memory allocating fds\n");
			exit(EXIT_FAILURE);
		}
	}
	sock->poll_index = sel->num_fds;
	fd = &sel->fds[sel->num_fds++];
	fd->fd = tds_get_s(sock->tds);
	fd->events = events;
	fd->revents = 0;
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
pool_open_logfile(void)
{
	int fd;

	tds_append_mode = 0;
	tdsdump_topen(tds_dir_getenv(TDS_DIR("TDSDUMP")));

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

/* listening socket received from inetd/xinetd or systemd, if any */
static TDS_SYS_SOCKET inherited_listen_fd = INVALID_SOCKET;

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

	if (!TDS_IS_SOCKET_INVALID(inherited_listen_fd)) {
		s = inherited_listen_fd;
		inherited_listen_fd = INVALID_SOCKET;
		tds_socket_set_nonblocking(s);
	} else {
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
	}
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
	SELECT_INFO sel = { NULL, 0, 8 };
	int min_expire_left = -1;
	int rc;

	s = pool->listen_fd;
	wakeup = pool->wakeup_fd;

	/* add the listening socket to the read list */
	if (!TDS_RESIZE(sel.fds, sel.alloc_fds)) {
		fprintf(stderr, "Out of memory allocating fds\n");
		exit(EXIT_FAILURE);
	}
	sel.fds[0].fd = s;
	sel.fds[0].events = POLLIN;
	sel.fds[1].fd = wakeup;
	sel.fds[1].events = POLLIN;

	while (!got_sigterm) {

		sel.num_fds = 2;
		sel.fds[0].revents = 0;
		sel.fds[1].revents = 0;

		/* add the user sockets to the read list */
		DLIST_FOREACH(dlist_user, &pool->users, puser)
			pool_select_add_socket(&sel, &puser->sock);

		/* add the pool member sockets to the read list */
		DLIST_FOREACH(dlist_member, &pool->active_members, pmbr)
			pool_select_add_socket(&sel, &pmbr->sock);

		if (min_expire_left > 0)
			min_expire_left *= 1000;

		rc = poll(sel.fds, sel.num_fds, min_expire_left);
		if (TDS_UNLIKELY(rc < 0)) {
			char *errstr;

			if (sock_errno == TDSSOCK_EINTR)
				continue;
			errstr = sock_strerror(sock_errno);
			fprintf(stderr, "Error: poll returned %d, %s\n", sock_errno, errstr);
			sock_strerror_free(errstr);
			exit(EXIT_FAILURE);
		}
		if (TDS_UNLIKELY(got_sigterm))
			break;

#ifndef _WIN32
		if (TDS_UNLIKELY(got_sighup)) {
			got_sighup = false;
			pool_open_logfile();
		}
#endif

		/* process events */
		if ((sel.fds[1].revents & POLLIN) != 0) {
			char buf[32];
			READSOCKET(wakeup, buf, sizeof(buf));

			pool_process_events(pool);
		}

		/* process the sockets */
		if ((sel.fds[0].revents & POLLIN) != 0) {
			pool_user_create(pool, s);
		}
		pool_process_users(pool, sel.fds, sel.num_fds);
		min_expire_left = pool_process_members(pool, sel.fds, sel.num_fds);

		/* back from members */
		if (dlist_user_first(&pool->waiters))
			pool_schedule_waiters(pool);
	}			/* while !got_sigterm */
	tdsdump_log(TDS_DBG_INFO2, "Shutdown Requested\n");
}

static void
print_usage(const char *progname)
{
	fprintf(stderr, "Usage:\t%s [-l <log file>] [-c <conf file>] [-d] [-x] <pool name>\n", progname);
}

#ifndef _WIN32
/**
 * Check that a file descriptor is a listening socket.
 * \param fd file descriptor to check
 * \return true if fd is a listening socket or if it cannot be checked
 */
static bool
pool_is_listen_socket(int fd)
{
#ifdef SO_ACCEPTCONN
	int accept_conn = 0;
	SOCKLEN_T len = sizeof(accept_conn);

	if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, (void *) &accept_conn, &len) < 0 || !accept_conn)
		return false;
#endif
	return true;
}

/**
 * Set up the process when started by inetd/xinetd in "wait" mode.
 * In this mode the listening socket is passed as file descriptors 0, 1 and 2;
 * keep a copy of it and redirect standard streams to /dev/null.
 * Logging can be enabled with the -l option.
 * \return true on success, false on error
 */
static bool
pool_inetd_init(void)
{
	TDS_SYS_SOCKET s;
	int fd;

	if (!pool_is_listen_socket(0)) {
		fprintf(stderr, "Standard input is not a listening socket, check inetd service is configured with \"wait\"\n");
		return false;
	}

	s = dup(0);
	if (TDS_IS_SOCKET_INVALID(s)) {
		perror("dup");
		return false;
	}

	fd = open("/dev/null", O_RDWR);
	if (fd < 0) {
		perror("open /dev/null");
		CLOSESOCKET(s);
		return false;
	}
	fflush(stdout);
	fflush(stderr);
	while (dup2(fd, 0) < 0 && errno == EINTR)
		continue;
	while (dup2(fd, 1) < 0 && errno == EINTR)
		continue;
	while (dup2(fd, 2) < 0 && errno == EINTR)
		continue;
	if (fd > 2)
		close(fd);

	inherited_listen_fd = s;
	return true;
}

/* first file descriptor passed by systemd, see sd_listen_fds(3) */
#define SYSTEMD_LISTEN_FDS_START 3

/**
 * Detect systemd socket activation (see sd_listen_fds(3)).
 * The socket unit must have Accept=no and a single ListenStream.
 * \param found set to true if a listening socket was passed
 * \return true on success, false on error
 */
static bool
pool_systemd_init(bool *found)
{
	const char *env;
	char *end;
	long val;
	int fd = SYSTEMD_LISTEN_FDS_START;

	*found = false;

	/* the variables are for us only if LISTEN_PID is our pid */
	env = getenv("LISTEN_PID");
	if (!env)
		return true;
	val = strtol(env, &end, 10);
	if (end == env || *end || val != (long) getpid())
		return true;

	env = getenv("LISTEN_FDS");
	if (!env)
		return true;
	val = strtol(env, &end, 10);
	if (end == env || *end || val <= 0)
		return true;
	if (val != 1) {
		fprintf(stderr, "systemd passed %ld sockets, only one is supported\n", val);
		return false;
	}

	if (!pool_is_listen_socket(fd)) {
		fprintf(stderr, "File descriptor %d is not a listening socket, check systemd socket is configured with Accept=no\n", fd);
		return false;
	}

	/* do not leak the socket to other programs, as sd_listen_fds does */
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	inherited_listen_fd = fd;
	*found = true;
	return true;
}
#  define INETD_OPT "x"
#else
#  define INETD_OPT ""
#endif

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
#ifndef _WIN32
	bool inetd = false;
#endif
	TDS_POOL *pool;
	tds_dir_char *config_path = NULL;

	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigterm_handler);
#ifndef _WIN32
	signal(SIGHUP, sighup_handler);
	signal(SIGPIPE, SIG_IGN);
#endif

	while ((opt = getopt(argc, argv, "l:c:" DAEMON_OPT INETD_OPT)) != -1) {
		switch (opt) {
		case 'l':
			logfile_name = optarg;
			break;
#ifdef HAVE_FORK
		case 'd':
			daemonize = true;
			break;
#endif
		case 'c':
			config_path = tds_dir_from_cstr(optarg);
			if (!config_path) {
				fprintf(stderr, "Out of memory\n");
				return EXIT_FAILURE;
			}
			break;
#ifndef _WIN32
		case 'x':
			inetd = true;
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
#ifndef _WIN32
	if (inetd) {
#ifdef HAVE_FORK
		/* inetd waits for the process it started, it must not go away */
		if (daemonize) {
			fprintf(stderr, "Options -d and -x cannot be used together\n");
			return EXIT_FAILURE;
		}
#endif
		if (!pool_inetd_init())
			return EXIT_FAILURE;
	} else {
		bool found;

		if (!pool_systemd_init(&found))
			return EXIT_FAILURE;
		if (found)
			fprintf(stderr, "Using listening socket passed by systemd\n");
	}
#endif
	pool = pool_init(argv[optind], config_path);
	TDS_ZERO_FREE(config_path);
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
