/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Brian Bruns
 * Copyright (C) 2004-2015  Ziglio Frediano
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#include <freetds/time.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif /* HAVE_NETINET_TCP_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#if HAVE_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SELECT_H */

#if HAVE_POLL_H
#include <poll.h>
#endif /* HAVE_POLL_H */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif /* HAVE_SYS_EVENTFD_H */

#include <freetds/tds.h>
#include <freetds/string.h>
#include <freetds/tls.h>
#include "replacements.h"

#include <signal.h>
#include <assert.h>

/* error is always returned */
#define TDSSELERR   0
#define TDSPOLLURG 0x8000u

#if ENABLE_ODBC_MARS
static void tds_check_cancel(TDSCONNECTION *conn);
#endif


/**
 * \addtogroup network
 * @{ 
 */

#ifdef _WIN32
int
tds_socket_init(void)
{
	WSADATA wsadata;

	return WSAStartup(MAKEWORD(1, 1), &wsadata);
}

void
tds_socket_done(void)
{
	WSACleanup();
}
#endif

#if !defined(SOL_TCP) && (defined(IPPROTO_TCP) || defined(_WIN32))
/* fix incompatibility between MS headers */
# ifndef IPPROTO_TCP
#  define IPPROTO_TCP IPPROTO_TCP
# endif
# define SOL_TCP IPPROTO_TCP
#endif

/* Optimize the way we send packets */
#undef USE_CORK
#undef USE_NODELAY
/* On early Linux use TCP_CORK if available */
#if defined(__linux__) && defined(TCP_CORK)
#define USE_CORK 1
/* On *BSD try to use TCP_CORK */
/*
 * NOPUSH flag do not behave in the same way
 * cf ML "FreeBSD 5.0 performance problems with TCP_NOPUSH"
 */
#elif (defined(__FreeBSD__) || defined(__GNU_FreeBSD__) || defined(__OpenBSD__)) && defined(TCP_CORK)
#define USE_CORK 1
/* otherwise use NODELAY */
#elif defined(TCP_NODELAY) && defined(SOL_TCP)
#define USE_NODELAY 1
/* under VMS we have to define TCP_NODELAY */
#elif defined(__VMS)
#define TCP_NODELAY 1
#define USE_NODELAY 1
#endif

/**
 * Set socket to non-blocking
 * @param sock socket to set
 * @return 0 on success or error code
 */
int
tds_socket_set_nonblocking(TDS_SYS_SOCKET sock)
{
#if !defined(_WIN32)
	unsigned int ioctl_nonblocking = 1;
#else
	u_long ioctl_nonblocking = 1;
#endif

	if (IOCTLSOCKET(sock, FIONBIO, &ioctl_nonblocking) >= 0)
		return 0;
	return sock_errno;
}

static void
tds_addrinfo_set_port(struct addrinfo *addr, unsigned int port)
{
	assert(addr != NULL);

	switch(addr->ai_family) {
	case AF_INET:
		((struct sockaddr_in *) addr->ai_addr)->sin_port = htons(port);
		break;

#ifdef AF_INET6
	case AF_INET6:
		((struct sockaddr_in6 *) addr->ai_addr)->sin6_port = htons(port);
		break;
#endif
	}
}

const char*
tds_addrinfo2str(struct addrinfo *addr, char *name, int namemax)
{
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 0
#endif
	if (!name || namemax <= 0)
		return "";
	if (getnameinfo(addr->ai_addr, addr->ai_addrlen, name, namemax, NULL, 0, NI_NUMERICHOST) == 0)
		return name;
	name[0] = 0;
	return name;
}

static TDSERRNO
tds_connect_socket(TDSSOCKET *tds, struct addrinfo *addr, unsigned int port, int timeout, int *p_oserr)
{
	SOCKLEN_T optlen;
	TDSCONNECTION *conn = tds->conn;
	char ipaddr[128];

	int retval, len;

	tds_addrinfo_set_port(addr, port);
	tds_addrinfo2str(addr, ipaddr, sizeof(ipaddr));

	if (TDS_IS_SOCKET_INVALID(conn->s))
		return TDSECONN;

	*p_oserr = 0;

	tdsdump_log(TDS_DBG_INFO1, "Connecting to %s port %d (TDS version %d.%d)\n", 
			ipaddr, port,
			TDS_MAJOR(conn), TDS_MINOR(conn));

#ifdef  DOS32X			/* the other connection doesn't work  on WATTCP32 */
	if (connect(conn->s, addr->ai_addr, addr->ai_addrlen) < 0) {
		*p_oserr = sock_errno;
		tdsdump_log(TDS_DBG_ERROR, "tds_open_socket(): %s:%d", ipaddr, port);
		return TDSECONN;
	}
#else
	if (!timeout) {
		/* A timeout of zero means wait forever; 90,000 seconds will feel like forever. */
		timeout = 90000;
	}

	if ((*p_oserr = tds_socket_set_nonblocking(conn->s)) != 0) {
		tds_connection_close(conn);
		return TDSEUSCT; 	/* close enough: "Unable to set communications timer" */
	}
	retval = connect(conn->s, addr->ai_addr, addr->ai_addrlen);
	if (retval == 0) {
		tdsdump_log(TDS_DBG_INFO2, "connection established\n");
	} else {
		int err = *p_oserr = sock_errno;
		char *errstr = sock_strerror(err);
		tdsdump_log(TDS_DBG_ERROR, "tds_open_socket: connect(2) returned \"%s\"\n", errstr);
		sock_strerror_free(errstr);
#if DEBUGGING_CONNECTING_PROBLEM
		if (err != ECONNREFUSED && err != ENETUNREACH && err != TDSSOCK_EINPROGRESS) {
			tdsdump_dump_buf(TDS_DBG_ERROR, "Contents of sockaddr_in", addr->ai_addr, addr->ai_addrlen);
			tdsdump_log(TDS_DBG_ERROR, 	" sockaddr_in:\t"
							      "%s = %x\n" 
							"\t\t\t%s = %x\n" 
							"\t\t\t%s = %s\n"
							, "sin_family", addr->ai_family
							, "port", port
							, "address", ipaddr
							);
		}
#endif
		if (err != TDSSOCK_EINPROGRESS)
			return TDSECONN;
		
		*p_oserr = TDSSOCK_ETIMEDOUT;
		if (tds_select(tds, TDSSELWRITE|TDSSELERR, timeout) == 0)
			return TDSECONN;
	}
#endif	/* not DOS32X */

	/* check socket error */
	optlen = sizeof(len);
	len = 0;
	if (tds_getsockopt(conn->s, SOL_SOCKET, SO_ERROR, (char *) &len, &optlen) != 0) {
		char *errstr = sock_strerror(*p_oserr = sock_errno);
		tdsdump_log(TDS_DBG_ERROR, "getsockopt(2) failed: %s\n", errstr);
		sock_strerror_free(errstr);
		return TDSECONN;
	}
	if (len != 0) {
		char *errstr = sock_strerror(*p_oserr = len);
		tdsdump_log(TDS_DBG_ERROR, "getsockopt(2) reported: %s\n", errstr);
		sock_strerror_free(errstr);
		return TDSECONN;
	}

	return TDSEOK;
}

TDSERRNO
tds_open_socket(TDSSOCKET *tds, struct addrinfo *addr, unsigned int port, int timeout, int *p_oserr)
{
	TDSCONNECTION *conn = tds->conn;
	int len;
	TDSERRNO tds_error;

	*p_oserr = 0;

	conn->s = socket(addr->ai_family, SOCK_STREAM, 0);
	if (TDS_IS_SOCKET_INVALID(conn->s)) {
		char *errstr = sock_strerror(*p_oserr = sock_errno);
		tdsdump_log(TDS_DBG_ERROR, "socket creation error: %s\n", errstr);
		sock_strerror_free(errstr);
		return TDSESOCK;
	}
	tds->state = TDS_IDLE;

#ifdef SO_KEEPALIVE
	len = 1;
	setsockopt(conn->s, SOL_SOCKET, SO_KEEPALIVE, (const void *) &len, sizeof(len));
#endif

#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL)
	len = 40;
	setsockopt(conn->s, SOL_TCP, TCP_KEEPIDLE, (const void *) &len, sizeof(len));
	len = 2;
	setsockopt(conn->s, SOL_TCP, TCP_KEEPINTVL, (const void *) &len, sizeof(len));
#endif

#if defined(__APPLE__) && defined(SO_NOSIGPIPE)
	len = 1;
	if (setsockopt(conn->s, SOL_SOCKET, SO_NOSIGPIPE, (const void *) &len, sizeof(len))) {
		*p_oserr = sock_errno;
		tds_connection_close(conn);
		return TDSESOCK;
	}
#endif

	len = 1;
#if defined(USE_NODELAY)
	setsockopt(conn->s, SOL_TCP, TCP_NODELAY, (const void *) &len, sizeof(len));
#elif defined(USE_CORK)
	if (setsockopt(conn->s, SOL_TCP, TCP_CORK, (const void *) &len, sizeof(len)) < 0)
		setsockopt(conn->s, SOL_TCP, TCP_NODELAY, (const void *) &len, sizeof(len));
#else
#error One should be defined
#endif

	while ((tds_error = tds_connect_socket(tds, addr, port, timeout, p_oserr)) != TDSEOK) {
		addr = addr->ai_next;
		if (!addr) {
			tds_connection_close(conn);
			tdsdump_log(TDS_DBG_ERROR, "tds_open_socket() failed\n");
			return tds_error;
		}
	}

	tdsdump_log(TDS_DBG_INFO2, "tds_open_socket() succeeded\n");
	return TDSEOK;
}

/**
 * Close current socket.
 * For last socket close entire connection.
 * For MARS send FIN request.
 * This attempts a graceful disconnection, for ungraceful call
 * tds_connection_close.
 */
void
tds_close_socket(TDSSOCKET * tds)
{
	if (!IS_TDSDEAD(tds)) {
#if ENABLE_ODBC_MARS
		TDSCONNECTION *conn = tds->conn;
		unsigned n = 0, count = 0;
		tds_mutex_lock(&conn->list_mtx);
		for (; n < conn->num_sessions; ++n)
			if (TDSSOCKET_VALID(conn->sessions[n]))
				++count;
		if (count > 1)
			tds_append_fin(tds);
		tds_mutex_unlock(&conn->list_mtx);
		if (count <= 1) {
			tds_disconnect(tds);
			tds_connection_close(conn);
		} else {
			tds_set_state(tds, TDS_DEAD);
		}
#else
		tds_disconnect(tds);
		if (CLOSESOCKET(tds_get_s(tds)) == -1)
			tdserror(tds_get_ctx(tds), tds,  TDSECLOS, sock_errno);
		tds_set_s(tds, INVALID_SOCKET);
		tds_set_state(tds, TDS_DEAD);
#endif
	}
}

void
tds_connection_close(TDSCONNECTION *conn)
{
#if ENABLE_ODBC_MARS
	unsigned n = 0;
#endif

	if (!TDS_IS_SOCKET_INVALID(conn->s)) {
		/* TODO check error ?? how to return it ?? */
		CLOSESOCKET(conn->s);
		conn->s = INVALID_SOCKET;
	}

#if ENABLE_ODBC_MARS
	tds_mutex_lock(&conn->list_mtx);
	for (; n < conn->num_sessions; ++n)
		if (TDSSOCKET_VALID(conn->sessions[n]))
			tds_set_state(conn->sessions[n], TDS_DEAD);
	tds_mutex_unlock(&conn->list_mtx);
#else
	tds_set_state((TDSSOCKET* ) conn, TDS_DEAD);
#endif
}

/**
 * Select on a socket until it's available or the timeout expires. 
 * Meanwhile, call the interrupt function. 
 * \return	>0 ready descriptors
 *		 0 timeout 
 * 		<0 error (cf. errno).  Caller should  close socket and return failure. 
 * This function does not call tdserror or close the socket because it can't know the context in which it's being called.   
 */
int
tds_select(TDSSOCKET * tds, unsigned tds_sel, int timeout_seconds)
{
	int rc, seconds;
	unsigned int poll_seconds;

	assert(tds != NULL);
	assert(timeout_seconds >= 0);

	/* 
	 * The select loop.  
	 * If an interrupt handler is installed, we iterate once per second, 
	 * 	else we try once, timing out after timeout_seconds (0 == never). 
	 * If select(2) is interrupted by a signal (e.g. press ^C in sqsh), we timeout.
	 * 	(The application can retry if desired by installing a signal handler.)
	 *
	 * We do not measure current time against end time, to avoid being tricked by ntpd(8) or similar. 
	 * Instead, we just count down.  
	 *
	 * We exit on the first of these events:
	 * 1.  a descriptor is ready. (return to caller)
	 * 2.  select(2) returns an important error.  (return to caller)
	 * A timeout of zero says "wait forever".  We do that by passing a NULL timeval pointer to select(2). 
	 */
	poll_seconds = (tds_get_ctx(tds) && tds_get_ctx(tds)->int_handler)? 1 : timeout_seconds;
	for (seconds = timeout_seconds; timeout_seconds == 0 || seconds > 0; seconds -= poll_seconds) {
		struct pollfd fds[2];
		int timeout = poll_seconds ? poll_seconds * 1000 : -1;

		if (TDS_IS_SOCKET_INVALID(tds_get_s(tds)))
			return -1;

		if ((tds_sel & TDSSELREAD) != 0 && tds->conn->tls_session && tds_ssl_pending(tds->conn))
			return POLLIN;

		fds[0].fd = tds_get_s(tds);
		fds[0].events = tds_sel;
		fds[0].revents = 0;
		fds[1].fd = tds_wakeup_get_fd(&tds->conn->wakeup);
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		rc = poll(fds, 2, timeout);

		if (rc > 0 ) {
			if (fds[0].revents & POLLERR) {
				set_sock_errno(TDSSOCK_ECONNRESET);
				return -1;
			}
			rc = fds[0].revents;
			if (fds[1].revents) {
#if ENABLE_ODBC_MARS
				tds_check_cancel(tds->conn);
#endif
				rc |= TDSPOLLURG;
			}
			return rc;
		}

		if (rc < 0) {
			char *errstr;

			switch (sock_errno) {
			case TDSSOCK_EINTR:
				/* FIXME this should be global maximun, not loop one */
				seconds += poll_seconds;
				break;	/* let interrupt handler be called */
			default: /* documented: EFAULT, EBADF, EINVAL */
				errstr = sock_strerror(sock_errno);
				tdsdump_log(TDS_DBG_ERROR, "error: poll(2) returned %d, \"%s\"\n",
						sock_errno, errstr);
				sock_strerror_free(errstr);
				return rc;
			}
		}

		assert(rc == 0 || (rc < 0 && sock_errno == TDSSOCK_EINTR));

		if (tds_get_ctx(tds) && tds_get_ctx(tds)->int_handler) {	/* interrupt handler installed */
			/*
			 * "If hndlintr() returns INT_CANCEL, DB-Library sends an attention token [TDS_BUFSTAT_ATTN]
			 * to the server. This causes the server to discontinue command processing. 
			 * The server may send additional results that have already been computed. 
			 * When control returns to the mainline code, the mainline code should do 
			 * one of the following: 
			 * - Flush the results using dbcancel 
			 * - Process the results normally"
			 */
			int timeout_action = (*tds_get_ctx(tds)->int_handler) (tds_get_parent(tds));
			switch (timeout_action) {
			case TDS_INT_CONTINUE:		/* keep waiting */
				continue;
			case TDS_INT_CANCEL:		/* abort the current command batch */
							/* FIXME tell tds_goodread() not to call tdserror() */
				return 0;
			default:
				tdsdump_log(TDS_DBG_NETWORK, 
					"tds_select: invalid interupt handler return code: %d\n", timeout_action);
				return -1;
			}
		}
		/* 
		 * We can reach here if no interrupt handler was installed and we either timed out or got EINTR. 
		 * We cannot be polling, so we are about to drop out of the loop. 
		 */
		assert(poll_seconds == timeout_seconds);
	}
	
	return 0;
}

/**
 * Read from an OS socket
 * @TODO remove tds, save error somewhere, report error in another way
 * @returns 0 if blocking, <0 error >0 bytes read
 */
static int
tds_socket_read(TDSCONNECTION * conn, TDSSOCKET *tds, unsigned char *buf, int buflen)
{
	int len, err;

#if ENABLE_EXTRA_CHECKS
	/* this simulate the fact that recv can return less bytes */
	if (buflen >= 5) {
		static int cnt = 0;
		if (++cnt == 5) {
			cnt = 0;
			buflen -= 3;
		}
	}
#endif

	/* read directly from socket*/
	len = READSOCKET(conn->s, buf, buflen);
	if (len > 0)
		return len;

	err = sock_errno;
	if (len < 0 && TDSSOCK_WOULDBLOCK(err))
		return 0;

	/* detect connection close */
	tds_connection_close(conn);
	tdserror(conn->tds_ctx, tds, len == 0 ? TDSESEOF : TDSEREAD, len == 0 ? 0 : err);
	return -1;
}

/**
 * Write to an OS socket
 * @returns 0 if blocking, <0 error >0 bytes readed
 */
static int
tds_socket_write(TDSCONNECTION *conn, TDSSOCKET *tds, const unsigned char *buf, int buflen)
{
	int err, len;
	char *errstr;

#if ENABLE_EXTRA_CHECKS
	/* this simulate the fact that send can return less bytes */
	if (buflen >= 5) {
		static int cnt = 0;
		if (++cnt == 5) {
			cnt = 0;
			buflen -= 3;
		}
	}
#endif

#if defined(__APPLE__) && defined(SO_NOSIGPIPE)
	len = send(conn->s, buf, buflen, 0);
#else
	len = WRITESOCKET(conn->s, buf, buflen);
#endif
	if (len > 0)
		return len;

	err = sock_errno;
	if (0 == len || TDSSOCK_WOULDBLOCK(err))
		return 0;

	assert(len < 0);

	/* detect connection close */
	errstr = sock_strerror(err);
	tdsdump_log(TDS_DBG_NETWORK, "send(2) failed: %d (%s)\n", err, errstr);
	sock_strerror_free(errstr);
	tds_connection_close(conn);
	tdserror(conn->tds_ctx, tds, TDSEWRIT, err);
	return -1;
}

int
tds_wakeup_init(TDSPOLLWAKEUP *wakeup)
{
	TDS_SYS_SOCKET sv[2];
	int ret;

	wakeup->s_signal = wakeup->s_signaled = INVALID_SOCKET;
#if defined(__linux__) && HAVE_EVENTFD
#  ifdef EFD_CLOEXEC
	ret = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
#  else
	ret = -1;
#  endif
	/* Linux version up to 2.6.26 do not support flags, try without */
	if (ret < 0 && (ret = eventfd(0, 0)) >= 0) {
		fcntl(ret, F_SETFD, fcntl(ret, F_GETFD, 0) | FD_CLOEXEC);
		fcntl(ret, F_SETFL, fcntl(ret, F_GETFL, 0) | O_NONBLOCK);
	}
	if (ret >= 0) {
		wakeup->s_signaled = ret;
		return 0;
	}
#endif
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	if (ret)
		return ret;
	wakeup->s_signal   = sv[0];
	wakeup->s_signaled = sv[1];
	return 0;
}

void
tds_wakeup_close(TDSPOLLWAKEUP *wakeup)
{
	if (!TDS_IS_SOCKET_INVALID(wakeup->s_signal))
		CLOSESOCKET(wakeup->s_signal);
	if (!TDS_IS_SOCKET_INVALID(wakeup->s_signaled))
		CLOSESOCKET(wakeup->s_signaled);
}


void
tds_wakeup_send(TDSPOLLWAKEUP *wakeup, char cancel)
{
#if defined(__linux__) && HAVE_EVENTFD
	if (wakeup->s_signal == -1) {
		TDS_UINT8 one = 1;
		(void) write(wakeup->s_signaled, &one, sizeof(one));
		return;
	}
#endif
	send(wakeup->s_signal, &cancel, sizeof(cancel), 0);
}

static int
tds_connection_signaled(TDSCONNECTION *conn)
{
	int len;
	char to_cancel[16];

#if defined(__linux__) && HAVE_EVENTFD
	if (conn->wakeup.s_signal == -1)
		return read(conn->wakeup.s_signaled, to_cancel, 8) > 0;
#endif

	len = READSOCKET(conn->wakeup.s_signaled, to_cancel, sizeof(to_cancel));
	do {
		/* no cancel found */
		if (len <= 0)
			return 0;
	} while(!to_cancel[--len]);
	return 1;
}

#if ENABLE_ODBC_MARS
static void
tds_check_cancel(TDSCONNECTION *conn)
{
	TDSSOCKET *tds;
	int rc;

	if (!tds_connection_signaled(conn))
		return;

	do {
		unsigned n = 0;

		rc = TDS_SUCCESS;
		tds_mutex_lock(&conn->list_mtx);
		/* Here we scan all list searching for sessions that should send cancel packets */
		for (; n < conn->num_sessions; ++n)
			if (TDSSOCKET_VALID(tds=conn->sessions[n]) && tds->in_cancel == 1) {
				/* send cancel */
				tds->in_cancel = 2;
				tds_mutex_unlock(&conn->list_mtx);
				rc = tds_append_cancel(tds);
				tds_mutex_lock(&conn->list_mtx);
				if (rc != TDS_SUCCESS)
					break;
			}
		tds_mutex_unlock(&conn->list_mtx);
		/* for all failed */
		/* this must be done outside loop cause it can alter list */
		/* this must be done unlocked cause it can lock again */
		if (rc != TDS_SUCCESS)
			tds_close_socket(tds);
	} while(rc != TDS_SUCCESS);
}
#endif

/**
 * Loops until we have received some characters
 * return -1 on failure
 */
int
tds_goodread(TDSSOCKET * tds, unsigned char *buf, int buflen)
{
	if (tds == NULL || buf == NULL || buflen < 1)
		return -1;

	for (;;) {
		int len, err;

		/* FIXME this block writing from other sessions */
		len = tds_select(tds, TDSSELREAD, tds->query_timeout);
#if !ENABLE_ODBC_MARS
		if (len > 0 && (len & TDSPOLLURG)) {
			tds_connection_signaled(tds->conn);
			/* send cancel */
			if (tds->in_cancel == 1)
				tds_put_cancel(tds);
			continue;
		}
#endif
		if (len > 0) {
			len = tds_socket_read(tds->conn, tds, buf, buflen);
			if (len == 0)
				continue;
			return len;
		}

		/* error */
		if (len < 0) {
			if (TDSSOCK_WOULDBLOCK(sock_errno)) /* shouldn't happen, but OK */
				continue;
			err = sock_errno;
			tds_connection_close(tds->conn);
			tdserror(tds_get_ctx(tds), tds, TDSEREAD, err);
			return -1;
		}

		/* timeout */
		switch (tdserror(tds_get_ctx(tds), tds, TDSETIME, sock_errno)) {
		case TDS_INT_CONTINUE:
			break;
		default:
		case TDS_INT_CANCEL:
			tds_close_socket(tds);
			return -1;
		}
	}
}

int
tds_connection_read(TDSSOCKET * tds, unsigned char *buf, int buflen)
{
	TDSCONNECTION *conn = tds->conn;

	if (conn->tls_session)
		return tds_ssl_read(conn, buf, buflen);

#if ENABLE_ODBC_MARS
	return tds_socket_read(conn, tds, buf, buflen);
#else
	return tds_goodread(tds, buf, buflen);
#endif
}

/**
 * \param tds the famous socket
 * \param buffer data to send
 * \param buflen bytes in buffer
 * \param last 1 if this is the last packet, else 0
 * \return length written (>0), <0 on failure
 */
int
tds_goodwrite(TDSSOCKET * tds, const unsigned char *buffer, size_t buflen)
{
	int len;
	size_t sent = 0;

	assert(tds && buffer);

	while (sent < buflen) {
		/* TODO if send buffer is full we block receive !!! */
		len = tds_select(tds, TDSSELWRITE, tds->query_timeout);

		if (len > 0) {
			len = tds_socket_write(tds->conn, tds, buffer + sent, buflen - sent);
			if (len == 0)
				continue;
			if (len < 0)
				return len;

			sent += len;
			continue;
		}

		/* error */
		if (len < 0) {
			int err = sock_errno;
			char *errstr;

			if (TDSSOCK_WOULDBLOCK(err)) /* shouldn't happen, but OK, retry */
				continue;
			errstr = sock_strerror(err);
			tdsdump_log(TDS_DBG_NETWORK, "select(2) failed: %d (%s)\n", err, errstr);
			sock_strerror_free(errstr);
			tds_connection_close(tds->conn);
			tdserror(tds_get_ctx(tds), tds, TDSEWRIT, err);
			return -1;
		}

		/* timeout */
		tdsdump_log(TDS_DBG_NETWORK, "tds_goodwrite(): timed out, asking client\n");
		switch (tdserror(tds_get_ctx(tds), tds, TDSETIME, sock_errno)) {
		case TDS_INT_CONTINUE:
			break;
		default:
		case TDS_INT_CANCEL:
			tds_close_socket(tds);
			return -1;
		}
	}

	return (int) sent;
}

void
tds_socket_flush(TDS_SYS_SOCKET sock)
{
#ifdef USE_CORK
	int opt;
	opt = 0;
	setsockopt(sock, SOL_TCP, TCP_CORK, (const void *) &opt, sizeof(opt));
	opt = 1;
	setsockopt(sock, SOL_TCP, TCP_CORK, (const void *) &opt, sizeof(opt));
#endif
}

int
tds_connection_write(TDSSOCKET *tds, const unsigned char *buf, int buflen, int final)
{
	int sent;
	TDSCONNECTION *conn = tds->conn;

#if !defined(_WIN32) && !defined(MSG_NOSIGNAL) && !defined(DOS32X) && (!defined(__APPLE__) || !defined(SO_NOSIGPIPE))
	void (*oldsig) (int);

	oldsig = signal(SIGPIPE, SIG_IGN);
	if (oldsig == SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't set SIGPIPE signal to be ignored\n");
	}
#endif

	if (conn->tls_session)
		sent = tds_ssl_write(conn, buf, buflen);
	else
#if ENABLE_ODBC_MARS
		sent = tds_socket_write(conn, tds, buf, buflen);
#else
		sent = tds_goodwrite(tds, buf, buflen);
#endif

	/* force packet flush */
	if (final && sent >= buflen)
		tds_socket_flush(tds_get_s(tds));

#if !defined(_WIN32) && !defined(MSG_NOSIGNAL) && !defined(DOS32X) && (!defined(__APPLE__) || !defined(SO_NOSIGPIPE))
	if (signal(SIGPIPE, oldsig) == SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't reset SIGPIPE signal to previous value\n");
	}
#endif
	return sent;
}

/**
 * Get port of all instances
 * @return default port number or 0 if error
 * @remark experimental, cf. MC-SQLR.pdf.
 */
int
tds7_get_instance_ports(FILE *output, struct addrinfo *addr)
{
	int num_try;
	struct pollfd fd;
	int retval;
	TDS_SYS_SOCKET s;
	char msg[16*1024];
	int msg_len = 0;
	int port = 0;
	char ipaddr[128];


	tds_addrinfo_set_port(addr, 1434);
	tds_addrinfo2str(addr, ipaddr, sizeof(ipaddr));

	tdsdump_log(TDS_DBG_ERROR, "tds7_get_instance_ports(%s)\n", ipaddr);

	/* create an UDP socket */
	if (TDS_IS_SOCKET_INVALID(s = socket(addr->ai_family, SOCK_DGRAM, 0))) {
		char *errstr = sock_strerror(sock_errno);
		tdsdump_log(TDS_DBG_ERROR, "socket creation error: %s\n", errstr);
		sock_strerror_free(errstr);
		return 0;
	}

	/*
	 * on cluster environment is possible that reply packet came from
	 * different IP so do not filter by ip with connect
	 */

	if (tds_socket_set_nonblocking(s) != 0) {
		CLOSESOCKET(s);
		return 0;
	}

	/* 
	 * Request the instance's port from the server.  
	 * There is no easy way to detect if port is closed so we always try to
	 * get a reply from server 16 times. 
	 */
	for (num_try = 0; num_try < 16 && msg_len == 0; ++num_try) {
		/* send the request */
		msg[0] = 3;
		if (sendto(s, msg, 1, 0, addr->ai_addr, addr->ai_addrlen) < 0)
			break;

		fd.fd = s;
		fd.events = POLLIN;
		fd.revents = 0;

		retval = poll(&fd, 1, 1000);
		
		/* on interrupt ignore */
		if (retval < 0 && sock_errno == TDSSOCK_EINTR)
			continue;
		
		if (retval == 0) { /* timed out */
#if 1
			tdsdump_log(TDS_DBG_ERROR, "tds7_get_instance_port: timed out on try %d of 16\n", num_try);
			continue;
#else
			int rc;
			tdsdump_log(TDS_DBG_INFO1, "timed out\n");

			switch(rc = tdserror(NULL, NULL, TDSETIME, 0)) {
			case TDS_INT_CONTINUE:
				continue;	/* try again */

			default:
				tdsdump_log(TDS_DBG_ERROR, "error: client error handler returned %d\n", rc);
			case TDS_INT_CANCEL: 
				CLOSESOCKET(s);
				return 0;
			}
#endif
		}
		if (retval < 0)
			break;

		/* got data, read and parse */
		if ((msg_len = recv(s, msg, sizeof(msg) - 1, 0)) > 3 && msg[0] == 5) {
			char *name, sep[2] = ";", *save;

			/* assure null terminated */
			msg[msg_len] = 0;
			tdsdump_dump_buf(TDS_DBG_INFO1, "instance info", msg, msg_len);
			
			if (0) {	/* To debug, print the whole string. */
				char *p;

				for (*sep = '\n', p=msg+3; p < msg + msg_len; p++) {
					if( *p == ';' )
						*p = *sep;
				}
				fputs(msg + 3, output);
			}

			/*
			 * Parse and print message.
			 */
			name = strtok_r(msg+3, sep, &save);
			while (name && output) {
				int i;
				static const char *const names[] = { "ServerName", "InstanceName", "IsClustered", "Version",
							       "tcp", "np", "via" };

				for (i=0; name && i < TDS_VECTOR_SIZE(names); i++) {
					const char *value = strtok_r(NULL, sep, &save);
					
					if (strcmp(name, names[i]) != 0)
						fprintf(output, "error: expecting '%s', found '%s'\n", names[i], name);
					if (value) 
						fprintf(output, "%15s %s\n", name, value);
					else 
						break;

					name = strtok_r(NULL, sep, &save);

					if (name && strcmp(name, names[0]) == 0)
						break;
				}
				if (name) 
					fprintf(output, "\n");
			}
		}
	}
	CLOSESOCKET(s);
	tdsdump_log(TDS_DBG_ERROR, "default instance port is %d\n", port);
	return port;
}

/**
 * Get port of given instance
 * @return port number or 0 if error
 */
int
tds7_get_instance_port(struct addrinfo *addr, const char *instance)
{
	int num_try;
	struct pollfd fd;
	int retval;
	TDS_SYS_SOCKET s;
	char msg[1024];
	int msg_len;
	int port = 0;
	char ipaddr[128];

	tds_addrinfo_set_port(addr, 1434);
	tds_addrinfo2str(addr, ipaddr, sizeof(ipaddr));

	tdsdump_log(TDS_DBG_ERROR, "tds7_get_instance_port(%s, %s)\n", ipaddr, instance);

	/* create an UDP socket */
	if (TDS_IS_SOCKET_INVALID(s = socket(addr->ai_family, SOCK_DGRAM, 0))) {
		char *errstr = sock_strerror(sock_errno);
		tdsdump_log(TDS_DBG_ERROR, "socket creation error: %s\n", errstr);
		sock_strerror_free(errstr);
		return 0;
	}

	/*
	 * on cluster environment is possible that reply packet came from
	 * different IP so do not filter by ip with connect
	 */

	if (tds_socket_set_nonblocking(s) != 0) {
		CLOSESOCKET(s);
		return 0;
	}

	/* 
	 * Request the instance's port from the server.  
	 * There is no easy way to detect if port is closed so we always try to
	 * get a reply from server 16 times. 
	 */
	for (num_try = 0; num_try < 16; ++num_try) {
		/* send the request */
		msg[0] = 4;
		strlcpy(msg + 1, instance, sizeof(msg) - 1);
		if (sendto(s, msg, (int)strlen(msg) + 1, 0, addr->ai_addr, addr->ai_addrlen) < 0)
			break;

		fd.fd = s;
		fd.events = POLLIN;
		fd.revents = 0;

		retval = poll(&fd, 1, 1000);
		
		/* on interrupt ignore */
		if (retval < 0 && sock_errno == TDSSOCK_EINTR)
			continue;
		
		if (retval == 0) { /* timed out */
#if 1
			tdsdump_log(TDS_DBG_ERROR, "tds7_get_instance_port: timed out on try %d of 16\n", num_try);
			continue;
#else
			int rc;
			tdsdump_log(TDS_DBG_INFO1, "timed out\n");

			switch(rc = tdserror(NULL, NULL, TDSETIME, 0)) {
			case TDS_INT_CONTINUE:
				continue;	/* try again */

			default:
				tdsdump_log(TDS_DBG_ERROR, "error: client error handler returned %d\n", rc);
			case TDS_INT_CANCEL: 
				CLOSESOCKET(s);
				return 0;
			}
#endif
		}
		if (retval < 0)
			break;

		/* TODO pass also connection and set instance/servername ?? */

		/* got data, read and parse */
		if ((msg_len = recv(s, msg, sizeof(msg) - 1, 0)) > 3 && msg[0] == 5) {
			char *p;
			long l = 0;
			int instance_ok = 0, port_ok = 0;

			/* assure null terminated */
			msg[msg_len] = 0;
			tdsdump_dump_buf(TDS_DBG_INFO1, "instance info", msg, msg_len);

			/*
			 * Parse message and check instance name and port.
			 * We don't check servername cause it can be very different from the client's. 
			 */
			for (p = msg + 3;;) {
				char *name, *value;

				name = p;
				p = strchr(p, ';');
				if (!p)
					break;
				*p++ = 0;

				value = name;
				if (*name) {
					value = p;
					p = strchr(p, ';');
					if (!p)
						break;
					*p++ = 0;
				}

				if (strcasecmp(name, "InstanceName") == 0) {
					if (strcasecmp(value, instance) != 0)
						break;
					instance_ok = 1;
				} else if (strcasecmp(name, "tcp") == 0) {
					l = strtol(value, &p, 10);
					if (l > 0 && l <= 0xffff && *p == 0)
						port_ok = 1;
				}
			}
			if (port_ok && instance_ok) {
				port = l;
				break;
			}
		}
	}
	CLOSESOCKET(s);
	tdsdump_log(TDS_DBG_ERROR, "instance port is %d\n", port);
	return port;
}

#if defined(_WIN32)
static const char tds_unknown_wsaerror[] = "undocumented WSA error code";

char *
tds_prwsaerror(int erc)
{
	char *errstr = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, NULL, erc,
		      MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&errstr, 0, NULL);
	if (errstr) {
		size_t len = strlen(errstr);
		while (len > 0 && (errstr[len-1] == '\r' || errstr[len-1] == '\n'))
			errstr[len-1] = 0;
		return errstr;
	}
	return (char*) tds_unknown_wsaerror;
}

void
tds_prwsaerror_free(char *s)
{
	if (s != tds_unknown_wsaerror)
		LocalFree((HLOCAL) s);
}
#endif

/** @} */

