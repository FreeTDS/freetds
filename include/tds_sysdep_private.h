/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

static char rcsid_tds_sysdep_private_h[] = "$Id: tds_sysdep_private.h,v 1.10 2003-10-24 10:11:11 freddy77 Exp $";
static void *no_unused_tds_sysdep_private_h_warn[] = { rcsid_tds_sysdep_private_h, no_unused_tds_sysdep_private_h_warn };

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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#define READSOCKET(a,b,c)	recv((a), (b), (c), 0L)
#define WRITESOCKET(a,b,c)	send((a), (b), (c), 0L)
#define CLOSESOCKET(a)		closesocket((a))
#define IOCTLSOCKET(a,b,c)	ioctlsocket((a), (b), (c))
#define NETDB_REENTRANT 1	/* BSD-style netdb interface is reentrant */

#ifndef EINTR
#define EINTR WSAEINTR
#endif
#define EINPROGRESS WSAEINPROGRESS
#define getpid() GetCurrentThreadId()
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

#endif /* defined(WIN32) || defined(_WIN32) || defined(__WIN32__) */

#ifndef sock_errno
#define sock_errno errno
#endif

#ifndef READSOCKET
#define READSOCKET(a,b,c)	read((a), (b), (c))
#endif /* !READSOCKET */

#ifndef WRITESOCKET
#define WRITESOCKET(a,b,c)	write((a), (b), (c))
#endif /* !WRITESOCKET */

#ifndef CLOSESOCKET
#define CLOSESOCKET(a)		close((a))
#endif /* !CLOSESOCKET */

#ifndef IOCTLSOCKET
#define IOCTLSOCKET(a,b,c)	ioctl((a), (b), (c))
#endif /* !IOCTLSOCKET */

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif /* _tds_sysdep_private_h_ */
