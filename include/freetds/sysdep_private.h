/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2010  Frediano Ziglio
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

#ifndef _tdsguard_gbdINUKdHN7rAOavGyKkWw_
#define _tdsguard_gbdINUKdHN7rAOavGyKkWw_

#define TDS_ADDITIONAL_SPACE 16

#ifdef MSG_NOSIGNAL
# define TDS_NOSIGNAL MSG_NOSIGNAL
#else
# define TDS_NOSIGNAL 0L
#endif

#ifdef __cplusplus
#define TDS_EXTERN_C extern "C"
#else
#define TDS_EXTERN_C
#endif

#ifdef __INCvxWorksh
#include <ioLib.h>		/* for FIONBIO */
#endif				/* __INCvxWorksh */

#if defined(DOS32X)
#define READSOCKET(a,b,c)	recv((a), (b), (c), TDS_NOSIGNAL)
#define WRITESOCKET(a,b,c)	send((a), (b), (c), TDS_NOSIGNAL)
#define CLOSESOCKET(a)		closesocket((a))
#define IOCTLSOCKET(a,b,c)	ioctlsocket((a), (b), (char*)(c))
#define SOCKLEN_T int
#define select select_s
typedef int pid_t;
#define strcasecmp stricmp
#define strncasecmp strnicmp
/* TODO this has nothing to do with ip ... */
#define getpid() _gethostid()
#endif	/* defined(DOS32X) */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
#include <freetds/windows.h>
#define READSOCKET(a,b,c)	recv((a), (char *) (b), (c), TDS_NOSIGNAL)
#define WRITESOCKET(a,b,c)	send((a), (const char *) (b), (c), TDS_NOSIGNAL)
#define CLOSESOCKET(a)		closesocket((a))
#define IOCTLSOCKET(a,b,c)	ioctlsocket((a), (b), (c))
#define SOCKLEN_T int
TDS_EXTERN_C int  tds_socket_init(void);
#define INITSOCKET()	tds_socket_init()
TDS_EXTERN_C void tds_socket_done(void);
#define DONESOCKET()	tds_socket_done()
#define NETDB_REENTRANT 1	/* BSD-style netdb interface is reentrant */

#define TDSSOCK_EINTR WSAEINTR
#define TDSSOCK_EINPROGRESS WSAEWOULDBLOCK
#define TDSSOCK_ETIMEDOUT WSAETIMEDOUT
#define TDSSOCK_WOULDBLOCK(e) ((e)==WSAEWOULDBLOCK)
#define TDSSOCK_ECONNRESET WSAECONNRESET
#define sock_errno WSAGetLastError()
#define set_sock_errno(err) WSASetLastError(err)
#define sock_strerror(n) tds_prwsaerror(n)
#define sock_strerror_free(s) tds_prwsaerror_free(s)
#ifndef __MINGW32__
typedef DWORD pid_t;
#endif
#undef strcasecmp
#define strcasecmp stricmp
#undef strncasecmp
#define strncasecmp strnicmp
#if defined(HAVE__SNPRINTF) && !defined(HAVE_SNPRINTF)
#define snprintf _snprintf
#endif

#ifndef WIN32
#define WIN32 1
#endif

#if defined(_WIN64) && !defined(WIN64)
#define WIN64 1
#endif

/* use macros to use new style names */
#if defined(__MSVCRT__) || defined(_MSC_VER)
/* Use API as always present and not causing problems */
#undef getpid
#define getpid()           GetCurrentProcessId()
#define strdup(s)          _strdup(s)
#define unlink(f)          _unlink(f)
#define putenv(s)          _putenv(s)
#undef fileno
#define fileno(f)          _fileno(f)
#undef close
#define close(f)           _close(f)
#undef open
#define open(fn,...)       _open(fn,__VA_ARGS__)
#undef dup2
#define dup2(o,n)          _dup2(o,n)
#define stricmp(s1,s2)     _stricmp(s1,s2)
#define strnicmp(s1,s2,n)  _strnicmp(s1,s2,n)
#endif

#endif /* defined(WIN32) || defined(_WIN32) || defined(__WIN32__) */

#ifndef sock_errno
#define sock_errno errno
#endif

#ifndef set_sock_errno
#define set_sock_errno(err) do { errno = (err); } while(0)
#endif

#ifndef sock_strerror
#define sock_strerror(n) strerror(n)
#define sock_strerror_free(s) do {} while(0)
#endif

#ifndef TDSSOCK_EINTR
#define TDSSOCK_EINTR EINTR
#endif

#ifndef TDSSOCK_EINPROGRESS 
#define TDSSOCK_EINPROGRESS EINPROGRESS
#endif

#ifndef TDSSOCK_ETIMEDOUT
#define TDSSOCK_ETIMEDOUT ETIMEDOUT
#endif

#ifndef TDSSOCK_WOULDBLOCK
# if defined(EWOULDBLOCK) && EAGAIN != EWOULDBLOCK
#  define TDSSOCK_WOULDBLOCK(e) ((e)==EAGAIN||(e)==EWOULDBLOCK)
# else
#  define TDSSOCK_WOULDBLOCK(e) ((e)==EAGAIN)
# endif
#endif

#ifndef TDSSOCK_ECONNRESET
#define TDSSOCK_ECONNRESET ECONNRESET
#endif

#ifndef INITSOCKET
#define INITSOCKET()	0
#endif /* !INITSOCKET */

#ifndef DONESOCKET
#define DONESOCKET()	do { } while(0)
#endif /* !DONESOCKET */

#ifndef READSOCKET
# ifdef MSG_NOSIGNAL
#  define READSOCKET(s,b,l)	recv((s), (b), (l), MSG_NOSIGNAL)
# else
#  define READSOCKET(s,b,l)	read((s), (b), (l))
# endif
#endif /* !READSOCKET */

#ifndef WRITESOCKET
# ifdef MSG_NOSIGNAL
#  define WRITESOCKET(s,b,l)	send((s), (b), (l), MSG_NOSIGNAL)
# else
#  define WRITESOCKET(s,b,l)	write((s), (b), (l))
# endif
#endif /* !WRITESOCKET */

#ifndef CLOSESOCKET
#define CLOSESOCKET(s)		close((s))
#endif /* !CLOSESOCKET */

#ifndef IOCTLSOCKET
#define IOCTLSOCKET(s,b,l)	ioctl((s), (b), (l))
#endif /* !IOCTLSOCKET */

#ifndef SOCKLEN_T
# define SOCKLEN_T socklen_t
#endif

#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
typedef int TDS_SYS_SOCKET;
#define INVALID_SOCKET -1
#define TDS_IS_SOCKET_INVALID(s) ((s) < 0)
#else
typedef SOCKET TDS_SYS_SOCKET;
#define TDS_IS_SOCKET_INVALID(s) ((s) == INVALID_SOCKET)
#endif

#define tds_accept      accept
#define tds_getpeername getpeername
#define tds_getsockopt  getsockopt
#define tds_getsockname getsockname
#define tds_recvfrom    recvfrom

#if defined(__hpux__) && SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
# if HAVE__XPG_ACCEPT
#  undef tds_accept
#  define tds_accept _xpg_accept
# elif HAVE___ACCEPT
#  undef tds_accept
#  define tds_accept __accept
# endif
# if HAVE__XPG_GETPEERNAME
#  undef tds_getpeername
#  define tds_getpeername _xpg_getpeername
# elif HAVE___GETPEERNAME
#  undef tds_getpeername
#  define tds_getpeername __getpeername
# endif
# if HAVE__XPG_GETSOCKOPT
#  undef tds_getsockopt
#  define tds_getsockopt _xpg_getsockopt
# elif HAVE___GETSOCKOPT
#  undef tds_getsockopt
#  define tds_getsockopt __getsockopt
# endif
# if HAVE__XPG_GETSOCKNAME
#  undef tds_getsockname
#  define tds_getsockname _xpg_getsockname
# elif HAVE___GETSOCKNAME
#  undef tds_getsockname
#  define tds_getsockname __getsockname
# endif
# if HAVE__XPG_RECVFROM
#  undef tds_recvfrom
#  define tds_recvfrom _xpg_recvfrom
# elif HAVE___RECVFROM
#  undef tds_recvfrom
#  define tds_recvfrom __recvfrom
# endif
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef PRId64
#define PRId64 TDS_I64_PREFIX "d"
#endif
#ifndef PRIu64
#define PRIu64 TDS_I64_PREFIX "u"
#endif
#ifndef PRIx64
#define PRIx64 TDS_I64_PREFIX "x"
#endif

#ifndef UINT64_C
# if SIZEOF_INT >= 8
#  define UINT64_C(c) c ## U
#  define INT64_C(c) c
# elif SIZEOF_LONG >= 8
#  define UINT64_C(c) c ## UL
#  define INT64_C(c) c ## L
# elif SIZEOF_LONG_LONG >= 8
#  define UINT64_C(c) c ## ULL
#  define INT64_C(c) c ## LL
# elif SIZEOF___INT64 >= 8
#  define UINT64_C(c) c ## ui64
#  define INT64_C(c) c ## i64
# else
#  error Unable to understand how to define 64 bit constants
# endif
#endif

#include <freetds/sysdep_types.h>

#endif /* _tdsguard_gbdINUKdHN7rAOavGyKkWw_ */
