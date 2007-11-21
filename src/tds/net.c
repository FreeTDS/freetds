/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Brian Bruns
 * Copyright (C) 2004, 2005, 2006, 2007  Ziglio Frediano
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

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

#include "tds.h"
#include "tdsstring.h"
#include "replacements.h"

#include <signal.h>
#include <assert.h>

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#elif defined(HAVE_OPENSSL)
#include <openssl/ssl.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: net.c,v 1.70 2007-11-21 04:28:31 jklowden Exp $");

static int tds_select(TDSSOCKET * tds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, int timeout_seconds);


/**
 * \addtogroup network
 * @{ 
 */

#ifdef WIN32
int
_tds_socket_init(void)
{
	WSADATA wsadata;

	return WSAStartup(MAKEWORD(1, 1), &wsadata);
}

void
_tds_socket_done(void)
{
	WSACleanup();
}
#endif

#if !defined(SOL_TCP) && defined(IPPROTO_TCP)
#define SOL_TCP IPPROTO_TCP
#endif

/* Optimize the way we send packets */
#undef USE_MSGMORE
#undef USE_CORK
#undef USE_NODELAY
/* On Linux 2.4.x we can use MSG_MORE */
#if defined(__linux__) && defined(MSG_MORE)
#define USE_MSGMORE 1
/* On early Linux use TCP_CORK if available */
#elif defined(__linux__) && defined(TCP_CORK)
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

#if !defined(WIN32)
typedef unsigned int ioctl_nonblocking_t;
#else
typedef u_long ioctl_nonblocking_t;
#endif

int
tds_open_socket(TDSSOCKET * tds, const char *ip_addr, unsigned int port, int timeout)
{
	struct sockaddr_in sin;
#if !defined(DOS32X)
	ioctl_nonblocking_t ioctl_nonblocking;
	int retval;
#endif
	int len;
	int tds_error = TDSECONN;
	char ip[20];
#if defined(DOS32X) || defined(WIN32)
	int optlen;
#else
	socklen_t optlen;
#endif

	memset(&sin, 0, sizeof(sin));

	sin.sin_addr.s_addr = inet_addr(ip_addr);
	if (sin.sin_addr.s_addr == INADDR_NONE) {
		tdsdump_log(TDS_DBG_ERROR, "inet_addr() failed, IP = %s\n", ip_addr);
		return TDS_FAIL;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	tdsdump_log(TDS_DBG_INFO1, "Connecting to %s port %d (TDS version %d.%d)\n", 
			tds_inet_ntoa_r(sin.sin_addr, ip, sizeof(ip)), ntohs(sin.sin_port), 
			tds->major_version, tds->minor_version);

	if (TDS_IS_SOCKET_INVALID(tds->s = socket(AF_INET, SOCK_STREAM, 0))) {
		tdserror(tds->tds_ctx, tds, TDSESOCK, 0);  
		tdsdump_log(TDS_DBG_ERROR, "socket creation error: %s\n", strerror(sock_errno));
		return TDS_FAIL;
	}

#ifdef SO_KEEPALIVE
	len = 1;
	setsockopt(tds->s, SOL_SOCKET, SO_KEEPALIVE, (const void *) &len, sizeof(len));
#endif

	len = 1;
#if defined(USE_NODELAY) || defined(USE_MSGMORE)
	setsockopt(tds->s, SOL_TCP, TCP_NODELAY, (const void *) &len, sizeof(len));
#elif defined(USE_CORK)
	if (setsockopt(tds->s, SOL_TCP, TCP_CORK, (const void *) &len, sizeof(len)) < 0)
		setsockopt(tds->s, SOL_TCP, TCP_NODELAY, (const void *) &len, sizeof(len));
#else
#error One should be defined
#endif

#ifdef  DOS32X			/* the other connection doesn't work  on WATTCP32 */
	if (connect(tds->s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		char *message;

		if (asprintf(&message, "tds_open_socket(): %s:%d", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port)) >= 0) {
			perror(message);
			free(message);
		}
		tds_close_socket(tds);
		tdserror(tds->tds_ctx, tds, TDSECONN, 0);
		return TDS_FAIL;
	}
#else
	if (!timeout) {
		/* A timeout of zero means wait forever; 90,000 seconds will feel like forever. */
		timeout = 90000;
	}

	/* enable non-blocking mode */
	ioctl_nonblocking = 1;
	if (IOCTLSOCKET(tds->s, FIONBIO, &ioctl_nonblocking) < 0) {
		tds_close_socket(tds);
		return TDS_FAIL;
	}

	retval = connect(tds->s, (struct sockaddr *) &sin, sizeof(sin));
	if (retval == 0) {
		tdsdump_log(TDS_DBG_INFO2, "connection established\n");
	} else {
		fd_set fds;

		tdsdump_log(TDS_DBG_ERROR, "tds_open_socket: connect(2) returned \"%s\"\n", strerror(sock_errno));
#if DEBUGGING_CONNECTING_PROBLEM
		if (sock_errno != ECONNREFUSED && sock_errno != ENETUNREACH && sock_errno != EINPROGRESS) {
			tdsdump_dump_buf(TDS_DBG_ERROR, "Contents of sockaddr_in", &sin, sizeof(sin));
			tdsdump_log(TDS_DBG_ERROR, 	" sockaddr_in:\t"
							      "%s = %x\n" 
							"\t\t\t%s = %x\n" 
							"\t\t\t%s = %x\n"
							"\t\t\t%s = '%s'\n"
							, "sin_family", sin.sin_family
							, "sin_port", sin.sin_port
							, "sin_addr.s_addr", sin.sin_addr.s_addr
							, "(param ip_addr)", ip_addr
							);
		}
#endif
		if (sock_errno != TDSSOCK_EINPROGRESS)
			goto not_available;
		
		FD_ZERO(&fds);
		if (tds_select(tds, NULL, &fds, &fds, timeout) <= 0) {
			tds_error = TDSESOCK;
			goto not_available;
		}
	}

#endif

	/* check socket error */
	optlen = sizeof(len);
	len = 0;
	if (getsockopt(tds->s, SOL_SOCKET, SO_ERROR, (char *) &len, &optlen) != 0) {
		tdsdump_log(TDS_DBG_ERROR, "getsockopt(2) failed: %s\n", strerror(sock_errno));
		goto not_available;
	}
	if (len != 0) {
		tdsdump_log(TDS_DBG_ERROR, "getsockopt(2) reported: %s\n", strerror(len));
		goto not_available;
	}

	tdsdump_log(TDS_DBG_ERROR, "tds_open_socket() succeeded\n");
	return TDS_SUCCEED;
	
    not_available:
	
	tds_close_socket(tds);
	tdserror(tds->tds_ctx, tds, tds_error, sock_errno);
	tdsdump_log(TDS_DBG_ERROR, "tds_open_socket() failed\n");
	return TDS_FAIL;
}

int
tds_close_socket(TDSSOCKET * tds)
{
	int rc = -1;

	if (!IS_TDSDEAD(tds)) {
		rc = CLOSESOCKET(tds->s);
		tds->s = INVALID_SOCKET;
		tds_set_state(tds, TDS_DEAD);
		if (-1 == rc) 
			tdserror(tds->tds_ctx, tds,  TDSECLOS, sock_errno);
	}
	return rc;
}

/**
 * Select on a socket until it's available or the timeout expires. 
 * Meanwhile, call the interrupt function. 
 * \return	>0 ready descriptors
 *		 0 timeout 
 * 		<0 error (cf. errno).  Caller should  close socket and return failure. 
 * This function does not call tdserror or close the socket because it can't know the context in which it's being called.   
 */
static int
tds_select(TDSSOCKET * tds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, int timeout_seconds)
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
	poll_seconds = (tds->tds_ctx && tds->tds_ctx->int_handler)? 1 : timeout_seconds;
	for (seconds = timeout_seconds; timeout_seconds == 0 || seconds > 0; seconds -= poll_seconds) {
		struct timeval tv, *ptv = poll_seconds? &tv : NULL;
		
		tv.tv_sec = poll_seconds;
		tv.tv_usec = 0; 

		if (readfds)
			FD_SET(tds->s, readfds);
		if (writefds)
			FD_SET(tds->s, writefds);
		if (exceptfds)
			FD_SET(tds->s, exceptfds);

		rc = select(tds->s + 1, readfds, writefds, exceptfds, ptv); 

		if (rc > 0 ) {
			return rc;
		}

		if (rc < 0) {
			switch (sock_errno) {
			case TDSSOCK_EINTR:
				break;	/* let interrupt handler be called */
			default: /* documented: EFAULT, EBADF, EINVAL */
				tdsdump_log(TDS_DBG_ERROR, "error: select(2) returned 0x%x, \"%s\"\n", 
						sock_errno, strerror(sock_errno));
				return rc;
			}
		}

		assert(rc == 0 || (rc < 0 && sock_errno == TDSSOCK_EINTR));

		if (tds->tds_ctx && tds->tds_ctx->int_handler) {	/* interrupt handler installed */
			/*
			 * "If hndlintr() returns INT_CANCEL, DB-Library sends an attention token [TDS_BUFSTAT_ATTN]
			 * to the server. This causes the server to discontinue command processing. 
			 * The server may send additional results that have already been computed. 
			 * When control returns to the mainline code, the mainline code should do 
			 * one of the following: 
			 * - Flush the results using dbcancel 
			 * - Process the results normally"
			 */
			int timeout_action = (*tds->tds_ctx->int_handler) (tds->parent);
#if 0
			tdsdump_log(TDS_DBG_ERROR, "tds_ctx->int_handler returned %d\n", timeout_action);
#endif
			switch (timeout_action) {
			case TDS_INT_CONTINUE:		/* keep waiting */
				continue;
			case TDS_INT_CANCEL:		/* abort the current command batch */
							/* FIXME tell tds_goodread() not to call tdserror() */
				return 0;
			default:
				tdsdump_log(TDS_DBG_NETWORK, 
					"tds_select: invalid interupt handler return code: %d\n", timeout_action);
				exit(EXIT_FAILURE);
				break;
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
 * Loops until we have received buflen characters
 * return -1 on failure
 * This function does not close the socket.  Maybe it should.  
 */
static int
tds_goodread(TDSSOCKET * tds, unsigned char *buf, int buflen, unsigned char unfinished)
{
	int rc, got = 0;
	fd_set rfds;

	if (buf == NULL || buflen < 1 || tds == NULL)
		return 0;

	for (;;) {
		int len;

		if (IS_TDSDEAD(tds))
			return -1;

		FD_ZERO(&rfds);
		if ((len = tds_select(tds, &rfds, NULL, NULL, tds->query_timeout)) > 0) {
			len = 0;
			if (FD_ISSET(tds->s, &rfds)) {
#ifndef MSG_NOSIGNAL
				len = READSOCKET(tds->s, buf + got, buflen);
#else
				len = recv(tds->s, buf + got, buflen, MSG_NOSIGNAL);
#endif
				if (len < 0 && sock_errno == EAGAIN)
					continue;
				/* detect connection close */
				if (len <= 0) {
					tdserror(tds->tds_ctx, tds, len == 0 ? TDSESEOF : TDSEREAD, sock_errno);
					tds_close_socket(tds);
					return -1;
 				}
			}
		} else if (len < 0) {
			if (sock_errno == EAGAIN) /* shouldn't happen, but OK */
				continue;
			tdserror(tds->tds_ctx, tds, TDSEREAD, sock_errno);
			tds_close_socket(tds);
			return -1;
		} else { /* timeout */
			switch (rc = tdserror(tds->tds_ctx, tds, TDSETIME, sock_errno)) {
			case TDS_INT_CONTINUE:
				continue;
			case TDS_INT_TIMEOUT:
				tds_send_cancel(tds);
				continue; /* fixme: or return? */
			default:
			case TDS_INT_CANCEL:
				tds_close_socket(tds);
				return -1;
			}
			assert(0); /* not reached */
		}

		got += len;
		buflen -= len;
		/* doing test here reduce number of syscalls required */
		if (buflen <= 0)
			break;

		if (unfinished && got)
			break;
	}
	return got;
}

static int
goodread(TDSSOCKET * tds, unsigned char *buf, int buflen)
{
#ifdef HAVE_GNUTLS
	if (tds->tls_session)
		return gnutls_record_recv(tds->tls_session, buf, buflen);
#elif defined(HAVE_OPENSSL)
	if (tds->tls_session)
		return SSL_read((SSL*) tds->tls_session, buf, buflen);
#endif
	return tds_goodread(tds, buf, buflen, 0);
}

/**
 * Read in one 'packet' from the server.  This is a wrapped outer packet of
 * the protocol (they bundle result packets into chunks and wrap them at
 * what appears to be 512 bytes regardless of how that breaks internal packet
 * up.   (tetherow\@nol.org)
 * @return bytes read or -1 on failure
 */
int
tds_read_packet(TDSSOCKET * tds)
{
	unsigned char header[8];
	int len, have;

	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_NETWORK, "Read attempt when state is TDS_DEAD");
		return -1;
	}

	/*
	 * Read in the packet header.  We use this to figure out our packet length. 
	 * Cast to int are needed because some compiler seem to convert
	 * len to unsigned (as FreeBSD 4.5 one)
	 */
	if ((len = goodread(tds, header, sizeof(header))) < (int) sizeof(header)) {
		/* GW ADDED */
		if (len < 0) {
			/* not needed because goodread() already called:  tdserror(tds->tds_ctx, tds, TDSEREAD, 0); */
			tds_close_socket(tds);
			tds->in_len = 0;
			tds->in_pos = 0;
			return -1;
		}

		/* GW ADDED */
		/*
		 * Not sure if this is the best way to do the error
		 * handling here but this is the way it is currently
		 * being done.
		 */

		tds->in_len = 0;
		tds->in_pos = 0;
		tds->last_packet = 1;
		if (tds->state != TDS_IDLE && len == 0) {
			tds_close_socket(tds);
		}
		return -1;
	}
	tdsdump_dump_buf(TDS_DBG_NETWORK, "Received header", header, sizeof(header));

#if 0
	/*
	 * Note:
	 * this was done by Gregg, I don't think its the real solution (it breaks
	 * under 5.0, but I haven't gotten a result big enough to test this yet.
 	 */
	if (IS_TDS42(tds)) {
		if (header[0] != 0x04 && header[0] != 0x0f) {
			tdsdump_log(TDS_DBG_ERROR, "Invalid packet header %d\n", header[0]);
			/*
			 * Not sure if this is the best way to do the error 
			 * handling here but this is the way it is currently 
			 * being done.
			 */
			tds->in_len = 0;
			tds->in_pos = 0;
			tds->last_packet = 1;
			return (-1);
		}
	}
#endif

	/* Convert our packet length from network to host byte order */
	len = ((((unsigned int) header[2]) << 8) | header[3]) - 8;

	/*
	 * If this packet size is the largest we have gotten allocate space for it
	 */
	if (len > tds->in_buf_max) {
		unsigned char *p;

		if (!tds->in_buf) {
			p = (unsigned char *) malloc(len);
		} else {
			p = (unsigned char *) realloc(tds->in_buf, len);
		}
		if (!p) {
			tds_close_socket(tds);
			return -1;
		}
		tds->in_buf = p;
		/* Set the new maximum packet size */
		tds->in_buf_max = len;
	}

	/* Clean out the in_buf so we don't use old stuff by mistake */
	memset(tds->in_buf, 0, tds->in_buf_max);

	/* Now get exactly how many bytes the server told us to get */
	have = 0;
	while (have < len) {
		int nbytes = goodread(tds, tds->in_buf + have, len - have);
		if (nbytes < 1) {
			/*
			 * Not sure if this is the best way to do the error
			 * handling here but this is the way it is currently
			 * being done.
			 */
			/* no need to call tdserror(), because goodread() already did */
			tds->in_len = 0;
			tds->in_pos = 0;
			tds->last_packet = 1;
			tds_close_socket(tds);
			return -1;
		}
		have += nbytes;
	}

	/* Set the last packet flag */
	tds->last_packet = (header[1] != 0);

	/* set the received packet type flag */
	tds->in_flag = header[0];

	/* Set the length and pos (not sure what pos is used for now */
	tds->in_len = have;
	tds->in_pos = 0;
	tdsdump_dump_buf(TDS_DBG_NETWORK, "Received packet", tds->in_buf, tds->in_len);

	return (tds->in_len);
}

/**
 * \param tds the famous socket
 * \param p pointer to buffer
 * \param len bytes in buffer
 * \param last 1 if this is the last packet, else 0
 * \return len on success, <0 on failure
 */
static int
tds_goodwrite(TDSSOCKET * tds, const unsigned char *p, int len, unsigned char last)
{
	int remaining = len;
	int nput, rc, err=0;
	fd_set fds;

	/* Fix of SIGSEGV when FD_SET() called with negative fd (Sergey A. Cherukhin, 23/09/2005) */
	if (TDS_IS_SOCKET_INVALID(tds->s))
		return -1;

	while (remaining > 0) {
		FD_ZERO(&fds);
		if ((rc = tds_select(tds, NULL, &fds, NULL, tds->query_timeout)) > 0) {
			nput = 0;
			if (FD_ISSET(tds->s, &fds)) {
#ifdef USE_MSGMORE
				nput = send(tds->s, p, remaining, last ? MSG_NOSIGNAL : MSG_NOSIGNAL|MSG_MORE);
				/* In case the kernel does not support MSG_MORE, try again without it */
				if (nput < 0 && errno == EINVAL && !last)
					nput = send(tds->s, p, remaining, MSG_NOSIGNAL);
#elif !defined(MSG_NOSIGNAL)
				nput = WRITESOCKET(tds->s, p, remaining);
#else
				nput = send(tds->s, p, remaining, MSG_NOSIGNAL);
#endif
				if (nput < 0 && sock_errno == EAGAIN)
					continue;
				/* detect connection close */
				if (nput <= 0) {
					tdserror(tds->tds_ctx, tds, nput == 0 ? TDSESEOF : TDSEWRIT, sock_errno);
					tds_close_socket(tds);
					return -1;
 				}
			}
		} else if (rc < 0) {
			if (sock_errno == EAGAIN) /* shouldn't happen, but OK, retry */
				continue;
			tdsdump_log(TDS_DBG_NETWORK, "TDS: Write failed in tds_write_packet\nError: %d (%s)\n", err, strerror(err));
			tdserror(tds->tds_ctx, tds, TDSEWRIT, sock_errno);
			tds_close_socket(tds);
			return -1;
		} else { /* timeout */
			tdsdump_log(TDS_DBG_NETWORK, "tds_goodwrite(): timed out, asking client\n");
			switch (rc = tdserror(tds->tds_ctx, tds, TDSETIME, sock_errno)) {
			case TDS_INT_CONTINUE:
				continue;
			case TDS_INT_TIMEOUT:
				/* FIXME we are not able to send a packet and we want to send a packet ?? */
				tds_send_cancel(tds);
				continue; /* fixme: or return? */
			default:
			case TDS_INT_CANCEL:
				tds_close_socket(tds);
				return -1;
			}
			assert(0); /* not reached */
		}

		p += nput;
		remaining -= nput;
	}

#ifdef USE_CORK
	/* force packet flush */
	if (last) {
		int opt;
		opt = 0;
		setsockopt(tds->s, SOL_TCP, TCP_CORK, (const void *) &opt, sizeof(opt));
		opt = 1;
		setsockopt(tds->s, SOL_TCP, TCP_CORK, (const void *) &opt, sizeof(opt));
	}
#endif

	return len;
}

int
tds_write_packet(TDSSOCKET * tds, unsigned char final)
{
	int sent;
	unsigned int left = 0;

#if !defined(WIN32) && !defined(MSG_NOSIGNAL) && !defined(DOS32X)
	void (*oldsig) (int);
#endif

#if TDS_ADDITIONAL_SPACE != 0
	if (tds->out_pos > tds->env.block_size) {
		left = tds->out_pos - tds->env.block_size;
		tds->out_pos = tds->env.block_size;
	}
#endif

	tds->out_buf[0] = tds->out_flag;
	tds->out_buf[1] = final;
	tds->out_buf[2] = (tds->out_pos) / 256u;
	tds->out_buf[3] = (tds->out_pos) % 256u;
	if (IS_TDS7_PLUS(tds) && !tds->connection)
		tds->out_buf[6] = 0x01;

	tdsdump_dump_buf(TDS_DBG_NETWORK, "Sending packet", tds->out_buf, tds->out_pos);

#if !defined(WIN32) && !defined(MSG_NOSIGNAL) && !defined(DOS32X)
	oldsig = signal(SIGPIPE, SIG_IGN);
	if (oldsig == SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't set SIGPIPE signal to be ignored\n");
	}
#endif

#ifdef HAVE_GNUTLS
	if (tds->tls_session)
		sent = gnutls_record_send(tds->tls_session, tds->out_buf, tds->out_pos);
	else
#elif defined(HAVE_OPENSSL)
	if (tds->tls_session)
		sent = SSL_write((SSL*) tds->tls_session, tds->out_buf, tds->out_pos);
	else
#endif
		sent = tds_goodwrite(tds, tds->out_buf, tds->out_pos, final);

#if !defined(WIN32) && !defined(MSG_NOSIGNAL) && !defined(DOS32X)
	if (signal(SIGPIPE, oldsig) == SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't reset SIGPIPE signal to previous value\n");
	}
#endif

#if TDS_ADDITIONAL_SPACE != 0
	memcpy(tds->out_buf + 8, tds->out_buf + tds->env.block_size, left);
#endif
	tds->out_pos = left + 8;

	/* GW added in check for write() returning <0 and SIGPIPE checking */
	return sent <= 0 ? TDS_FAIL : TDS_SUCCEED;
}

/**
 * Get port of given instance
 * @return port number or 0 if error
 */
int
tds7_get_instance_port(const char *ip_addr, const char *instance)
{
	int num_try;
	struct sockaddr_in sin;
	ioctl_nonblocking_t ioctl_nonblocking;
	struct timeval selecttimeout;
	fd_set fds;
	int retval;
	TDS_SYS_SOCKET s;
	char msg[1024];
	size_t msg_len;
	int port = 0;

	tdsdump_log(TDS_DBG_ERROR, "tds7_get_instance_port(%s, %s)\n", ip_addr, instance);

	sin.sin_addr.s_addr = inet_addr(ip_addr);
	if (sin.sin_addr.s_addr == INADDR_NONE) {
		tdsdump_log(TDS_DBG_ERROR, "inet_addr() failed, IP = %s\n", ip_addr);
		return 0;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(1434);

	/* create an UDP socket */
	if (TDS_IS_SOCKET_INVALID(s = socket(AF_INET, SOCK_DGRAM, 0))) {
		tdsdump_log(TDS_DBG_ERROR, "socket creation error: %s\n", strerror(sock_errno));
		return 0;
	}

	/*
	 * on cluster environment is possible that reply packet came from
	 * different IP so do not filter by ip with connect
	 */

	ioctl_nonblocking = 1;
	if (IOCTLSOCKET(s, FIONBIO, &ioctl_nonblocking) < 0) {
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
		tds_strlcpy(msg + 1, instance, sizeof(msg) - 1);
		sendto(s, msg, strlen(msg) + 1, 0, (struct sockaddr *) &sin, sizeof(sin));

		FD_ZERO(&fds);
		FD_SET(s, &fds);
		selecttimeout.tv_sec = 1;
		selecttimeout.tv_usec = 0;
		
		retval = select(s + 1, &fds, NULL, NULL, &selecttimeout);
		
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

				value = p;
				p = strchr(p, ';');
				if (!p)
					break;
				*p++ = 0;

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

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)

#ifdef HAVE_GNUTLS
static ssize_t 
tds_pull_func(gnutls_transport_ptr ptr, void* data, size_t len)
{
	TDSSOCKET *tds = (TDSSOCKET *) ptr;
#else
static int
tds_ssl_read(BIO *b, char* data, int len)
{
	TDSSOCKET *tds = (TDSSOCKET *) b->ptr;
#endif

	int have;

	tdsdump_log(TDS_DBG_INFO1, "in tds_pull_func\n");
	
	/* if we have some data send it */
	if (tds->out_pos > 8)
		tds_flush_packet(tds);

	if (tds->tls_session) {
		/* read directly from socket */
		return tds_goodread(tds, data, len, 1);
	}

	for(;;) {
		have = tds->in_len - tds->in_pos;
		tdsdump_log(TDS_DBG_INFO1, "have %d\n", have);
		assert(have >= 0);
		if (have > 0)
			break;
		tdsdump_log(TDS_DBG_INFO1, "before read\n");
		if (tds_read_packet(tds) < 0)
			return -1;
		tdsdump_log(TDS_DBG_INFO1, "after read\n");
	}
	if (len > have)
		len = have;
	tdsdump_log(TDS_DBG_INFO1, "read %d bytes\n", len);
	memcpy(data, tds->in_buf + tds->in_pos, len);
	tds->in_pos += len;
	return len;
}

#ifdef HAVE_GNUTLS
static ssize_t 
tds_push_func(gnutls_transport_ptr ptr, const void* data, size_t len)
{
	TDSSOCKET *tds = (TDSSOCKET *) ptr;
#else
static int
tds_ssl_write(BIO *b, const char* data, int len)
{
	TDSSOCKET *tds = (TDSSOCKET *) b->ptr;
#endif
	tdsdump_log(TDS_DBG_INFO1, "in tds_push_func\n");

	if (tds->tls_session) {
		/* write to socket directly */
		return tds_goodwrite(tds, data, len, 1);
	}
	tds_put_n(tds, data, len);
	return len;
}

#ifdef HAVE_GNUTLS

static void
tds_tls_log( int level, const char* s)
{
	tdsdump_log(TDS_DBG_INFO1, "GNUTLS: level %d:\n  %s", level, s);
}

static int tls_initialized = 0;

#ifdef TDS_ATTRIBUTE_DESTRUCTOR
static void __attribute__((destructor))
tds_tls_deinit(void)
{
	if (tls_initialized)
		gnutls_global_deinit();
}
#endif

int
tds_ssl_init(TDSSOCKET *tds)
{
	gnutls_session session;
	gnutls_certificate_credentials xcred;

	static const int kx_priority[] = {
		GNUTLS_KX_RSA_EXPORT, 
		GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS, GNUTLS_KX_DHE_RSA, 
		0
	};
	static const int cipher_priority[] = {
		GNUTLS_CIPHER_AES_256_CBC, GNUTLS_CIPHER_AES_128_CBC,
		GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR_128,
#if 0
		GNUTLS_CIPHER_ARCFOUR_40,
		GNUTLS_CIPHER_DES_CBC,
#endif
		0
	};
	static const int comp_priority[] = { GNUTLS_COMP_NULL, 0 };
	static const int mac_priority[] = {
		GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0
	};
	int ret;
	const char *tls_msg;

	xcred = NULL;
	session = NULL;	
	tls_msg = "initializing tls";

	/* FIXME place somewhere else, deinit at end */
	ret = 0;
	if (!tls_initialized)
		ret = gnutls_global_init();
	if (ret == 0) {
		tls_initialized = 1;

		gnutls_global_set_log_level(11);
		gnutls_global_set_log_function(tds_tls_log);
		tls_msg = "allocating credentials";
		ret = gnutls_certificate_allocate_credentials(&xcred);
	}

	if (ret == 0) {
		/* Initialize TLS session */
		tls_msg = "initializing session";
		ret = gnutls_init(&session, GNUTLS_CLIENT);
	}
	
	if (ret == 0) {
		gnutls_transport_set_ptr(session, tds);
		gnutls_transport_set_pull_function(session, tds_pull_func);
		gnutls_transport_set_push_function(session, tds_push_func);

		/* NOTE: there functions return int however they cannot fail */

		/* use default priorities... */
		gnutls_set_default_priority(session);

		/* ... but overwrite some */
		gnutls_cipher_set_priority(session, cipher_priority);
		gnutls_compression_set_priority(session, comp_priority);
		gnutls_kx_set_priority(session, kx_priority);
		gnutls_mac_set_priority(session, mac_priority);
		
		/* put the anonymous credentials to the current session */
		tls_msg = "setting credential";
		ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	}
	
	if (ret == 0) {
		/* Perform the TLS handshake */
		tls_msg = "handshake";
		ret = gnutls_handshake (session);
	}

	if (ret != 0) {
		if (session)
			gnutls_deinit(session);
		if (xcred)
			gnutls_certificate_free_credentials(xcred);
		tdsdump_log(TDS_DBG_ERROR, "%s failed: %s\n", tls_msg, gnutls_strerror (ret));
		return TDS_FAIL;
	}

	tdsdump_log(TDS_DBG_INFO1, "handshake succeeded!!\n");
	tds->tls_session = session;
	tds->tls_credentials = xcred;

	return TDS_SUCCEED;
}

void
tds_ssl_deinit(TDSSOCKET *tds)
{
	if (tds->tls_session) {
		gnutls_deinit(tds->tls_session);
		tds->tls_session = NULL;
	}
	if (tds->tls_credentials) {
		gnutls_certificate_free_credentials(tds->tls_credentials);
		tds->tls_credentials = NULL;
	}
}

#else
static long
tds_ssl_ctrl(BIO *b, int cmd, long num, void *ptr)
{
	TDSSOCKET *tds = (TDSSOCKET *) b->ptr;

	switch (cmd) {
	case BIO_CTRL_FLUSH:
		if (tds->out_pos > 8)
			tds_flush_packet(tds);
		return 1;
	}
	return 0;
}

static int
tds_ssl_free(BIO *a)
{
	/* nothing to do but required */
	return 1;
}

static BIO_METHOD tds_method =
{
	BIO_TYPE_MEM,
	"tds",
	tds_ssl_write,
	tds_ssl_read,
	NULL,
	NULL,
	tds_ssl_ctrl,
	NULL,
	tds_ssl_free,
	NULL,
};

static SSL_CTX *ssl_ctx;

static int
tds_init_openssl(void)
{
	SSL_METHOD *meth;

	SSL_library_init ();
	meth = TLSv1_client_method ();
	if (meth == NULL)
		return 1;
	ssl_ctx = SSL_CTX_new (meth);
	if (ssl_ctx == NULL)
		return 1;
	return 0;
}

#ifdef TDS_ATTRIBUTE_DESTRUCTOR
static void __attribute__((destructor))
tds_tls_deinit(void)
{
	if (ssl_ctx)
		SSL_CTX_free (ssl_ctx);
}
#endif

int
tds_ssl_init(TDSSOCKET *tds)
{
#define OPENSSL_CIPHERS \
	SSL3_TXT_RSA_DES_64_CBC_SHA " " \
	TLS1_TXT_RSA_EXPORT1024_WITH_RC4_56_SHA " " \
	TLS1_TXT_RSA_EXPORT1024_WITH_DES_CBC_SHA " " \
	SSL3_TXT_RSA_RC4_40_MD5 " " \
	SSL3_TXT_RSA_RC2_40_MD5 " " \
	SSL3_TXT_EDH_DSS_DES_64_CBC_SHA " " \
	TLS1_TXT_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA

	SSL *con;
	BIO *b;

	int ret;
	const char *tls_msg;

	con = NULL;
	b = NULL;
	tls_msg = "initializing tls";

	/* FIXME place somewhere else, deinit at end */
	ret = 0;
	if (!ssl_ctx)
		ret = tds_init_openssl();

	if (ret == 0) {
		/* Initialize TLS session */
		tls_msg = "initializing session";
		con = SSL_new(ssl_ctx);
	}
	
	if (con) {
		tls_msg = "creating bio";
		b = BIO_new(&tds_method);
	}

	ret = 0;
	if (b) {
		b->shutdown=1;
		b->init=1;
		b->num= -1;
		b->ptr = tds;
		SSL_set_bio(con, b, b);

		/* use priorities... */
		SSL_set_cipher_list(con, OPENSSL_CIPHERS);

		/* Perform the TLS handshake */
		tls_msg = "handshake";
		SSL_set_connect_state(con);
		ret = SSL_connect(con) != 1 || con->state != SSL_ST_OK;
	}

	if (ret != 0) {
		if (con) {
			SSL_shutdown(con);
			SSL_free(con);
		}
		tdsdump_log(TDS_DBG_ERROR, "%s failed\n", tls_msg);
		return TDS_FAIL;
	}

	tdsdump_log(TDS_DBG_INFO1, "handshake succeeded!!\n");
	tds->tls_session = con;
	tds->tls_credentials = NULL;

	return TDS_SUCCEED;
}

void
tds_ssl_deinit(TDSSOCKET *tds)
{
	if (tds->tls_session) {
		/* NOTE do not call SSL_shutdown here */
		SSL_free(tds->tls_session);
		tds->tls_session = NULL;
	}
}
#endif

#endif
/** @} */

