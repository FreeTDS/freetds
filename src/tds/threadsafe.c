/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
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
#ifndef WIN32
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "tds.h"
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: threadsafe.c,v 1.5.2.1 2002-09-21 07:45:15 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

char *
tds_timestamp_str(char *str, int maxlen)
{
struct tm  *tm;
time_t      t;
#ifdef __FreeBSD__
struct timeval  tv;
char usecs[10];
#endif
#ifdef _REENTRANT
struct tm res;
#endif

#ifdef __FreeBSD__
	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
#else
	/*
	* XXX Need to get a better time resolution for
	* non-FreeBSD systems.
	*/
	time(&t);
#endif

#ifdef _REENTRANT
	tm = localtime_r(&t, &res);
#else
	tm = localtime(&t);
#endif

	strftime(str, maxlen - 6, "%Y-%m-%d %H:%M:%S", tm);

#ifdef __FreeBSD__
    sprintf(usecs, ".%06lu", tv.tv_usec);
	strcat(str, usecs);
#endif

	return str;
}

/* test if original bsd socket api should considered reentrant */
#undef SOCK_REENTRANT
/* we don't care reentrancy */
#ifndef _REENTRANT
# define SOCK_REENTRANT 1
#else
/* tru64 and win32 "normal" api are reentrant */
# if (defined (__digital__) || defined(__osf__)) && defined (__unix__)
#  define SOCK_REENTRANT 1
# endif
# if defined(_WIN32) || defined(__WIN32__)
#  define SOCK_REENTRANT 1
# endif
#endif

struct hostent   *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#ifdef SOCK_REENTRANT
	return gethostbyname(servername);

#else

#if defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
	struct hostent result_buf;
	gethostbyname_r(servername, &result_buf, buffer, buflen, &result, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
	gethostbyname_r(servername, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
	struct hostent_data data;
	gethostbyname_r(servername, result, &data);
#elif defined(_REENTRANT)
#error gethostbyname_r style unknown
#endif

#endif
	return result;
}

struct hostent   *
tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{

#ifdef SOCK_REENTRANT
	return gethostbyaddr(addr, len, type);

#else

#if defined(HAVE_FUNC_GETHOSTBYADDR_R_8)
	struct hostent result_buf;
	gethostbyaddr_r(addr, len, type, &result_buf, buffer, buflen, &result, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_7)
	gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_5)
	struct hostent_data data;
	gethostbyaddr_r(addr, len, type, result, &data);
#else
#error gethostbyaddr_r style unknown
#endif

	return result;
#endif
}

struct servent *
tds_getservbyname_r(const char *name, char *proto, struct servent *result, char *buffer, int buflen)
{

#ifdef SOCK_REENTRANT
	return getservbyname(name, proto);

#else

#if defined(HAVE_FUNC_GETSERVBYNAME_R_6)
	struct servent result_buf;
	getservbyname_r(name, proto, &result_buf, buffer, buflen, &result);
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_5)
	getservbyname_r(name, proto, result, buffer, buflen);
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_4)
	struct servent_data data;
	getservbyname_r(name, proto, result, &data);
#else
#error getservbyname_r style unknown
#endif

	return result;

#endif
}

char *
tds_strtok_r(char *s, const char *delim, char **ptrptr)
{
#ifdef _REENTRANT
	return strtok_r(s, delim, ptrptr);
#else
	return strtok(s, delim);
#endif
}
