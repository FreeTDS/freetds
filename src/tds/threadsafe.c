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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

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

#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID)
#include <pwd.h>
#endif

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#include "tds.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: threadsafe.c,v 1.23 2002-12-06 21:56:59 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

char *
tds_timestamp_str(char *str, int maxlen)
{
	struct tm *tm;
	time_t t;

#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	char usecs[10];
#endif
#ifdef _REENTRANT
	struct tm res;
#endif

#if HAVE_GETTIMEOFDAY
	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
#else
	/*
	 * XXX Need to get a better time resolution for
	 * systems that don't have gettimeofday().
	 */
	time(&t);
#endif

#ifdef _REENTRANT
	tm = localtime_r(&t, &res);
#else
	tm = localtime(&t);
#endif

	strftime(str, maxlen - 6, "%Y-%m-%d %H:%M:%S", tm);

#if HAVE_GETTIMEOFDAY
	sprintf(usecs, ".%06lu", (long) tv.tv_usec);
	strcat(str, usecs);
#endif

	return str;
}

/*
 * If reentrant code was not requested, we don't care reentrancy, so
 * just assume the standard BSD netdb interface is reentrant and use it.
 */
#ifndef _REENTRANT
# ifndef NETDB_REENTRANT
#  define NETDB_REENTRANT 1
# endif	/* NETDB_REENTRANT */
#endif /* _REENTRANT */

struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#ifdef NETDB_REENTRANT
	return gethostbyname(servername);

#else

#if defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
	struct hostent result_buf;

	gethostbyname_r(servername, &result_buf, buffer, buflen, &result, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
	gethostbyname_r(servername, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
	struct hostent_data data;

	memset(&data, 0, sizeof(data));
#ifdef HAVE_SETHOSTENT_R
	sethostent_r(0, &data);
#endif
	gethostbyname_r(servername, result, &data);
#ifdef HAVE_ENDHOSTENT_R
	endhostent_r(&data);
#endif
#elif defined(_REENTRANT)
#error gethostbyname_r style unknown
#endif

#endif
	return result;
}

struct hostent *
tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{

#ifdef NETDB_REENTRANT
	return gethostbyaddr(addr, len, type);

#else

#if defined(HAVE_FUNC_GETHOSTBYADDR_R_8)
	struct hostent result_buf;

	gethostbyaddr_r(addr, len, type, &result_buf, buffer, buflen, &result, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_7)
	gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_5)
	struct hostent_data data;

	memset(&data, 0, sizeof(data));
#ifdef HAVE_SETHOSTENT_R
	sethostent_r(0, &data);
#endif
	gethostbyaddr_r(addr, len, type, result, &data);
#ifdef HAVE_ENDHOSTENT_R
	endhostent_r(&data);
#endif
#else
#error gethostbyaddr_r style unknown
#endif

	return result;
#endif
}

struct servent *
tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen)
{

#ifdef NETDB_REENTRANT
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

/**
 * Get user home directory
 * @return home directory or NULL if error. Should be freed with free
 */
char *
tds_get_homedir(void)
{
/* if is available getpwuid_r use it */
#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID_R)
	struct passwd *pw, bpw;
	char buf[1024];

#ifdef HAVE_FUNC_GETPWUID_R_5
	if (getpwuid_r(getuid(), &bpw, buf, sizeof(buf), &pw))
		return NULL;
#else
	if (!(pw=getpwuid_r(getuid(), &bpw, buf, sizeof(buf))))
		return NULL;
#endif
	return strdup(pw->pw_dir);
#else
/* if getpwuid is available use it for no reentrant (getpwuid is not reentrant) */
#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID) && !defined(_REENTRANT)
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (!pw)
		return NULL;
	return strdup(pw->pw_dir);
#else
	char *home;

	home = getenv("HOME");
	if (!home || !home[0])
		return NULL;
	return strdup(home);
#endif
#endif
}
