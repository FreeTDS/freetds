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
#include "tds.h"
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: threadsafe.c,v 1.1 2002-07-05 20:23:49 brianb Exp $";
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

struct hostent   *
tds_gethostbyname_r(char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#ifdef _REENTRANT
	return gethostbyname_r(servername, result, buffer, buflen, h_herrnop);
#else
	return gethostbyname(servername);
#endif
}

struct hostent   *
tds_gethostbyaddr_r(char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{

#ifdef _REENTRANT
	return gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_herrnop);
#else
	return gethostbyaddr(addr, len, type);
#endif
}

struct servent *
tds_getservbyname_r(char *name, char *proto, struct servent *result, char *buffer, int buflen)
{

#ifdef _REENTRANT
	return getservbyname_r(name, proto, type, result, buffer, buflen);
#else
	return getservbyname(name, proto);
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
