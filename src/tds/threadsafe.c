/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <shlobj.h>
#endif

#include "tds.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: threadsafe.c,v 1.48 2010-01-10 14:43:12 freddy77 Exp $");

char *
tds_timestamp_str(char *str, int maxlen)
{
#if !defined(_WIN32) && !defined(_WIN64)
	struct tm *tm;
	time_t t;

#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	char usecs[10];
#endif
#if defined(_REENTRANT)
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

#if defined(_REENTRANT)
#if HAVE_FUNC_LOCALTIME_R_TM
	tm = localtime_r(&t, &res);
#else
	tm = NULL;
	if (!localtime_r(&t, &res))
		tm = &res;
#endif /* HAVE_FUNC_LOCALTIME_R_TM */
#else
	tm = localtime(&t);
#endif

/**	strftime(str, maxlen - 6, "%Y-%m-%d %H:%M:%S", tm); **/
	strftime(str, maxlen - 6, "%H:%M:%S", tm);

#if HAVE_GETTIMEOFDAY
	sprintf(usecs, ".%06lu", (long) tv.tv_usec);
	strcat(str, usecs);
#endif

#else /* _WIN32 */
	SYSTEMTIME st;

	GetLocalTime(&st);
	_snprintf(str, maxlen - 1, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	str[maxlen - 1] = 0;
#endif

	return str;
}

/*
 * If reentrant code was not requested, we don't care reentrancy, so
 * just assume the standard BSD netdb interface is reentrant and use it.
 */
#ifndef _REENTRANT
# undef NETDB_REENTRANT
# define NETDB_REENTRANT 1
#endif /* _REENTRANT */


#if defined(NETDB_REENTRANT)
struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	return gethostbyname(servername);
}

#elif defined(HAVE_GETIPNODEBYNAME) || defined(HAVE_GETIPNODEBYADDR)
/**
 * Copy a hostent structure to an allocated buffer
 * @return 0 on success, -1 otherwise
 */
static int
tds_copy_hostent(struct hostent *he, struct hostent *result, char *buffer, int buflen)
{
#define CHECK_BUF(len) \
	if (p + sizeof(struct hostent) - buffer > buflen) return -1;
#define ALIGN_P do { p += TDS_ALIGN_SIZE - 1; p -= (p-buffer) % TDS_ALIGN_SIZE; } while(0)

	int n, i;
	char *p = buffer;
	struct hostent *he2;

	/* copy structure */
	he2 = result;
	memcpy(he2, he, sizeof(struct hostent));

	if (he->h_addr_list) {
		int len;
		char **addresses;

		/* count addresses */
		for (n = 0; he->h_addr_list[n]; ++n);

		/* copy addresses */
		addresses = (char **) p;
		he2->h_addr_list = (char **) p;
		len = sizeof(char *) * (n + 1);
		CHECK_BUF(len);
		p += len;
		ALIGN_P;
		for (i = 0; i < n; ++i) {
			addresses[i] = p;

			CHECK_BUF(he->h_length);
			memcpy(p, he->h_addr_list[i], he->h_length);
			p += he->h_length;
			ALIGN_P;
		}
		addresses[n] = NULL;
	}

	/* copy name */
	if (he->h_name) {
		n = strlen(he->h_name) + 1;
		he2->h_name = p;
		CHECK_BUF(n);
		memcpy(p, he->h_name, n);
		p += n;
		ALIGN_P;
	}

	if (he->h_aliases) {
		int len;
		char **aliases;

		/* count aliases */
		for (n = 0; he->h_aliases[n]; ++n);

		/* copy aliases */
		aliases = (char **) p;
		he2->h_aliases = (char **) p;
		len = sizeof(char *) * (n + 1);
		CHECK_BUF(len);
		p += len;
		for (i = 0; i < n; ++i) {
			len = strlen(he->h_aliases[i]) + 1;
			aliases[i] = p;

			CHECK_BUF(len);
			memcpy(p, he->h_aliases[i], len);
			p += len;
		}
		aliases[n] = NULL;
	}
	return 0;
}

struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	struct hostent *he = getipnodebyname(servername, AF_INET, 0, h_errnop);

	if (!he)
		return NULL;
	if (tds_copy_hostent(he, result, buffer, buflen)) {
		errno = ENOMEM;
		if (h_errnop)
			*h_errnop = NETDB_INTERNAL;
		freehostent(he);
		return NULL;
	}
	freehostent(he);
	return result;
}

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	if (gethostbyname_r(servername, result, buffer, buflen, &result, h_errnop))
		return NULL;
	return result;
}

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	result = gethostbyname_r(servername, result, buffer, buflen, h_errnop);
	return result;
}

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	struct hostent_data *data = (struct hostent_data *) buffer;

	memset(buffer, 0, buflen);
	if (gethostbyname_r(servername, result, data)) {
		*h_errnop = 0;
		result = NULL;
	}
	return result;
}

#elif defined(HAVE_GETADDRINFO) && \
	(!defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__) && \
	!defined(__bsdi__) && !defined(__DragonFly__))

static int
tds_addrinfo_to_hostent(struct addrinfo *ai, struct hostent *result, char *buffer, int buflen)
{
#define CHECK_BUF(len) \
	if (p + sizeof(struct hostent) - buffer > buflen) return -1;
#define ALIGN_P do { p += TDS_ALIGN_SIZE - 1; p -= (p-buffer) % TDS_ALIGN_SIZE; } while(0)

	int n;
	char *p = buffer;
	int len;
	char **addresses;
	struct addrinfo *curr_ai;

	memset(result, 0, sizeof(struct hostent));
	result->h_addrtype = sizeof(struct sockaddr_in);

	/* count addresses */
	for (n = 0, curr_ai = ai; curr_ai; curr_ai = curr_ai->ai_next) {
		if (curr_ai->ai_family != PF_INET)
			continue;
		++n;
	}

	/* copy addresses */
	addresses = (char **) p;
	result->h_addr_list = addresses;
	result->h_length = sizeof(struct in_addr);
	len = sizeof(char *) * (n + 1);
	CHECK_BUF(len);
	p += len;
	ALIGN_P;
	for (n = 0, curr_ai = ai; curr_ai; curr_ai = curr_ai->ai_next) {
		if (curr_ai->ai_family != PF_INET)
			continue;
		addresses[n++] = p;

		len = sizeof(struct in_addr);
		CHECK_BUF(len);
		memcpy(p, &((struct sockaddr_in *) curr_ai->ai_addr)->sin_addr, len);
		p += len;
		ALIGN_P;
	}
	addresses[n] = NULL;

	/* copy name */
	if (ai->ai_canonname) {
		n = strlen(ai->ai_canonname) + 1;
		result->h_name = p;
		CHECK_BUF(n);
		memcpy(p, ai->ai_canonname, n);
		p += n;
		ALIGN_P;
	}
	return 0;
}

struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_TCP;

	res = NULL;
	/* default error */
	if (h_errnop)
		*h_errnop = HOST_NOT_FOUND;
	if (getaddrinfo(servername, NULL, &hints, &res))
		return NULL;
	if (res->ai_family != PF_INET || !res->ai_addr) {
		freeaddrinfo(res);
		return NULL;
	}
	if (tds_addrinfo_to_hostent(res, result, buffer, buflen)) {
		errno = ENOMEM;
		if (h_errnop)
			*h_errnop = NETDB_INTERNAL;
		freeaddrinfo(res);
		return NULL;
	}
	freeaddrinfo(res);
	return result;
}

#elif defined(TDS_NO_THREADSAFE)
struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	return gethostbyname(servername);
}

#else
#error gethostbyname_r style unknown
#endif

/* not used by FreeTDS, uncomment if needed */
#ifdef ENABLE_DEVELOPING
struct hostent *
tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#if defined(NETDB_REENTRANT)
	return gethostbyaddr(addr, len, type);

#elif defined(HAVE_GETIPNODEBYADDR)
	struct hostent *he = getipnodebyaddr(addr, len, type, h_errnop);

	if (!he)
		return NULL;
	if (tds_copy_hostent(he, result, buffer, buflen)) {
		errno = ENOMEM;
		if (h_errnop)
			*h_errnop = NETDB_INTERNAL;
		freehostent(he);
		return NULL;
	}
	freehostent(he);
	return result;

#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_8)
	if (gethostbyaddr_r(addr, len, type, result, buffer, buflen, &result, h_errnop))
		return NULL;
	return result;

#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_7)
	result = gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_errnop);
	return result;

#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_5)
	struct hostent_data *data = (struct hostent_data *) buffer;

	memset(buffer, 0, buflen);
	if (gethostbyaddr_r(addr, len, type, result, data)) {
		*h_errnop = 0;
		result = NULL;
	}
	return result;

#elif defined(TDS_NO_THREADSAFE)
	return gethostbyaddr(addr, len, type);

#else
#error gethostbyaddr_r style unknown
#endif
}
#endif

const char *
tds_inet_ntoa_r(struct in_addr iaddr, char *ip, size_t len)
{
#if defined(AF_INET) && HAVE_INET_NTOP
	inet_ntop(AF_INET, &iaddr, ip, len);
#elif HAVE_INET_NTOA_R
	inet_ntoa_r(iaddr, ip, len);
#else
	tds_strlcpy(ip, inet_ntoa(iaddr), len);
#endif
	return ip;
}

struct servent *
tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen)
{
#if defined(NETDB_REENTRANT)
	return getservbyname(name, proto);

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_6)
	struct servent result_buf;

	getservbyname_r(name, proto, &result_buf, buffer, buflen, &result);
	return result;

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_5)
	getservbyname_r(name, proto, result, buffer, buflen);
	return result;

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_4)
	struct servent_data data;

	getservbyname_r(name, proto, result, &data);
	return result;

#elif defined(HAVE_GETADDRINFO)
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	res = NULL;
	if (getaddrinfo(NULL, name, &hints, &res))
		return NULL;
	if (res->ai_family != PF_INET || !res->ai_addr) {
		freeaddrinfo(res);
		return NULL;
	}
	memset(result, 0, sizeof(*result));
	result->s_port = ((struct sockaddr_in *) res->ai_addr)->sin_port;
	freeaddrinfo(res);
	return result;

#elif defined(TDS_NO_THREADSAFE)
	return getservbyname(name, proto);
#else
#error getservbyname_r style unknown
#endif
}

/**
 * Get user home directory
 * @return home directory or NULL if error. Should be freed with free
 */
char *
tds_get_homedir(void)
{
#ifndef _WIN32
/* if is available getpwuid_r use it */
#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID_R)
	struct passwd *pw, bpw;
	char buf[1024];

# if defined(HAVE_FUNC_GETPWUID_R_5)
	if (getpwuid_r(getuid(), &bpw, buf, sizeof(buf), &pw))
		return NULL;

# elif defined(HAVE_FUNC_GETPWUID_R_4_PW)
	if (!(pw = getpwuid_r(getuid(), &bpw, buf, sizeof(buf))))
		return NULL;
# else /* !HAVE_FUNC_GETPWUID_R_4_PW */
	if (getpwuid_r(getuid(), &bpw, buf, sizeof(buf)))
		return NULL;
	pw = &bpw;
# endif

	return strdup(pw->pw_dir);

/* if getpwuid is available use it for no reentrant (getpwuid is not reentrant) */
#elif defined(HAVE_GETUID) && defined(HAVE_GETPWUID) && !defined(_REENTRANT)
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
#else /* _WIN32 */
	/*
	 * For win32 we return application data cause we use "HOME" 
	 * only to store configuration files
	 */
	LPITEMIDLIST pidl;
	char path[MAX_PATH];
	HRESULT hr;
	LPMALLOC pMalloc = NULL;
	char * res = NULL;

	hr = SHGetMalloc(&pMalloc);
	if (!FAILED(hr)) {
		hr = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pidl);
		if (!FAILED(hr)) {
			if (SHGetPathFromIDList(pidl, path))
				res = strdup(path);
			(*pMalloc->lpVtbl->Free)(pMalloc, pidl);
		}
		(*pMalloc->lpVtbl->Release)(pMalloc);
	}
	return res;
#endif
}
