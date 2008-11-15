/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#ifndef _tds_sysdep_private_h_
#define _tds_sysdep_private_h_

/* $Id: tds_sysdep_private.h,v 1.26 2008-11-15 09:57:06 freddy77 Exp $ */

#undef TDS_RCSID
#if defined(__GNUC__) && __GNUC__ >= 3
#define TDS_RCSID(name, id) \
	static const char rcsid_##name[] __attribute__ ((unused)) = id
#else
#define TDS_RCSID(name, id) \
	static const char rcsid_##name[] = id; \
	static const void *const no_unused_##name##_warn[] = { rcsid_##name, no_unused_##name##_warn }
#endif

#define TDS_ADDITIONAL_SPACE 0

#ifdef MSG_NOSIGNAL
# define TDS_NOSIGNAL MSG_NOSIGNAL
#else
# define TDS_NOSIGNAL 0L
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
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
#define vsnprintf _vsnprintf
/* TODO this has nothing to do with ip ... */
#define getpid() _gethostid()
#endif	/* defined(DOS32X) */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#define READSOCKET(a,b,c)	recv((a), (char *) (b), (c), TDS_NOSIGNAL)
#define WRITESOCKET(a,b,c)	send((a), (const char *) (b), (c), TDS_NOSIGNAL)
#define CLOSESOCKET(a)		closesocket((a))
#define IOCTLSOCKET(a,b,c)	ioctlsocket((a), (b), (c))
#define SOCKLEN_T int
int  _tds_socket_init(void);
#define INITSOCKET()	_tds_socket_init()
void _tds_socket_done(void);
#define DONESOCKET()	_tds_socket_done()
#define NETDB_REENTRANT 1	/* BSD-style netdb interface is reentrant */

#define TDSSOCK_EINTR WSAEINTR
#define TDSSOCK_EINPROGRESS WSAEWOULDBLOCK
#define sock_errno WSAGetLastError()
#ifndef __MINGW32__
typedef DWORD pid_t;
#endif
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define atoll _atoi64
#define vsnprintf _vsnprintf

#ifndef WIN32
#define WIN32 1
#endif

#if defined(_WIN64) && !defined(WIN64)
#define WIN64 1
#endif

#define TDS_SDIR_SEPARATOR "\\"

/* use macros to use new style names */
#ifdef __MSVCRT__
#define getpid()           _getpid()
#define strdup(s)          _strdup(s)
#define stricmp(s1,s2)     _stricmp(s1,s2)
#define strnicmp(s1,s2,n)  _strnicmp(s1,s2,n)
#endif

#endif /* defined(WIN32) || defined(_WIN32) || defined(__WIN32__) */

#ifndef sock_errno
#define sock_errno errno
#endif

#ifndef TDSSOCK_EINTR
#define TDSSOCK_EINTR EINTR
#endif

#ifndef TDSSOCK_EINPROGRESS 
#define TDSSOCK_EINPROGRESS EINPROGRESS
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

#ifndef TDS_SDIR_SEPARATOR
#define TDS_SDIR_SEPARATOR "/"
#endif /* !TDS_SDIR_SEPARATOR */

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif /* _tds_sysdep_private_h_ */
