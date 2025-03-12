/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/time.h>

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

#if HAVE_ROKEN_H
#include <roken.h>
#endif /* HAVE_ROKEN_H */

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <shlobj.h>
#endif

#include <freetds/sysdep_private.h>
#include <freetds/utils.h>
#include <freetds/utils/path.h>
#include <freetds/thread.h>
#include <freetds/replacements.h>

struct tm *
tds_localtime_r(const time_t *timep, struct tm *result)
{
	struct tm *tm;

#if defined(_REENTRANT) && !defined(_WIN32)
#if HAVE_FUNC_LOCALTIME_R_TM
	tm = localtime_r(timep, result);
#else
	tm = NULL;
	if (!localtime_r(timep, result))
		tm = result;
#endif /* HAVE_FUNC_LOCALTIME_R_TM */
#else
	tm = localtime(timep);
	if (tm) {
		memcpy(result, tm, sizeof(*result));
		tm = result;
	}
#endif
	return tm;
}

char *
tds_timestamp_str(char *str, size_t maxlen)
{
#if !defined(_WIN32) && !defined(_WIN64)
	struct tm *tm;
	struct tm res;
	time_t t;

#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	char usecs[10];

	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
#else
	/*
	 * XXX Need to get a better time resolution for
	 * systems that don't have gettimeofday().
	 */
	time(&t);
#endif

	tm = tds_localtime_r(&t, &res);

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

#if 0
#undef HAVE_GETADDRINFO
#undef NETDB_REENTRANT
#undef HAVE_FUNC_GETSERVBYNAME_R_6
#undef HAVE_FUNC_GETSERVBYNAME_R_5
#undef HAVE_FUNC_GETSERVBYNAME_R_4
#undef TDS_NO_THREADSAFE

# if 0
#  define HAVE_FUNC_GETSERVBYNAME_R_6 1
int test_getservbyname_r(const char *name, const char *proto,
			 struct servent *result_buf, char *buffer,
			 size_t buflen, struct servent **result);
#  define getservbyname_r test_getservbyname_r
# elif 0
#  define HAVE_FUNC_GETSERVBYNAME_R_5 1
struct servent *
test_getservbyname_r(const char *name, const char *proto,
		     struct servent *result_buf, char *buffer,
		     size_t buflen);
#  define getservbyname_r test_getservbyname_r
# else
#  define HAVE_FUNC_GETSERVBYNAME_R_4 1
struct servent_data { int dummy; };
int
test_getservbyname_r(const char *name, const char *proto,
		     struct servent *result_buf,
		     struct servent_data *data);
#  define getservbyname_r test_getservbyname_r
# endif
#endif

/**
 * Return service port given the name
 */
int
tds_getservice(const char *name)
{
#if defined(HAVE_GETADDRINFO)
	/* new OSes should implement this in a proper way */
	struct addrinfo hints, *res;
	int result;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	res = NULL;
	if (getaddrinfo(NULL, name, &hints, &res))
		return 0;
	if (res->ai_family != AF_INET || !res->ai_addr) {
		freeaddrinfo(res);
		return 0;
	}
	result = ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);
	freeaddrinfo(res);
	return result;

#elif defined(NETDB_REENTRANT)
	/* HP-UX/Windows */
	struct servent *result = getservbyname(name, "tcp");
	return result ? ntohs(result->s_port) : 0;

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_6)
	/* Linux variant */
	struct servent *result = NULL;
	struct servent result_buf;
	char buffer[4096];

	if (!getservbyname_r(name, "tcp", &result_buf, buffer, sizeof(buffer), &result))
		return ntohs(result->s_port);
	return 0;

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_5)
	/* Solaris variant */
	struct servent result;
	char buffer[4096];

	if (getservbyname_r(name, "tcp", &result, buffer, sizeof(buffer)))
		return ntohs(result.s_port);
	return 0;

#elif defined(HAVE_FUNC_GETSERVBYNAME_R_4)
	/* AIX/BSD variant */
	struct servent result;
	struct servent_data data;

	if (!getservbyname_r(name, "tcp", &result, &data))
		return ntohs(result.s_port);
	return 0;

#elif defined(TDS_NO_THREADSAFE)
	struct servent *result = getservbyname(name, "tcp");
	return result ? ntohs(result->s_port) : 0;
#else
#error getservbyname_r style unknown
#endif
}

/**
 * Get user home directory
 * @return home directory or NULL if error. Should be freed with free
 */
tds_dir_char *
tds_get_homedir(void)
{
#ifndef _WIN32
/* if is available getpwuid_r use it */
#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID_R)
	struct passwd *pw, bpw;
	char buf[1024];

# if defined(HAVE_FUNC_GETPWUID_R_5)
	/* getpwuid_r can return 0 if uid is not found so check pw */
	pw = NULL;
	if (getpwuid_r(getuid(), &bpw, buf, sizeof(buf), &pw) || !pw)
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
	HRESULT hr;
	LPMALLOC pMalloc = NULL;
	tds_dir_char* res = NULL;

	hr = SHGetMalloc(&pMalloc);
	if (!FAILED(hr)) {
		LPITEMIDLIST pidl;
		hr = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pidl);
		if (!FAILED(hr)) {
			/*
			 * SHGetPathFromIDListA() tries to count the length of "path",
			 * so we have to make sure that it has only zeros; otherwise,
			 * invalid memory access is inevitable.
			 */
			tds_dir_char path[MAX_PATH] = TDS_DIR("");
			if (SHGetPathFromIDListW(pidl, path))
				res = tds_dir_dup(path);
			(*pMalloc->lpVtbl->Free)(pMalloc, pidl);
		}
		(*pMalloc->lpVtbl->Release)(pMalloc);
	}
	return res;
#endif
}
