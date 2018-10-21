/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2013 Peter Deacon
 * Copyright (C) 2013 Ziglio Frediano
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

#if !defined(HAVE_GETADDRINFO)
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NET_INET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#include <freetds/sysdep_private.h>
#include <freetds/utils.h>
#include "replacements.h"

static struct hostent *tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop);

/*
 * If reentrant code was not requested, we don't care reentrancy, so
 * just assume the standard BSD netdb interface is reentrant and use it.
 */
#ifndef _REENTRANT
# undef NETDB_REENTRANT
# define NETDB_REENTRANT 1
#endif /* _REENTRANT */

#if defined(NETDB_REENTRANT)
static struct hostent *
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
		for (n = 0; he->h_addr_list[n]; ++n)
			continue;

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
		for (n = 0; he->h_aliases[n]; ++n)
			continue;

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

static struct hostent *
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
static struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	if (gethostbyname_r(servername, result, buffer, buflen, &result, h_errnop))
		return NULL;
	return result;
}

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
static struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	result = gethostbyname_r(servername, result, buffer, buflen, h_errnop);
	return result;
}

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
static struct hostent *
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

#elif defined(TDS_NO_THREADSAFE)
static struct hostent *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	return gethostbyname(servername);
}

#else
#error gethostbyname_r style unknown
#endif

/* Incomplete implementation, single ipv4 addr, service does not work, hints do not work */
int
tds_getaddrinfo(const char *node, const char *service, const struct tds_addrinfo *hints, struct tds_addrinfo **res)
{
	struct tds_addrinfo *addr;
	struct sockaddr_in *sin = NULL;
	struct hostent *host;
	in_addr_t ipaddr;
	char buffer[4096];
	struct hostent result;
	int h_errnop, port = 0;

	assert(node != NULL);

	if ((addr = (tds_addrinfo *) calloc(1, sizeof(struct tds_addrinfo))) == NULL)
		goto Cleanup;

	if ((sin = (struct sockaddr_in *) calloc(1, sizeof(struct sockaddr_in))) == NULL)
		goto Cleanup;

	addr->ai_addr = (struct sockaddr *) sin;
	addr->ai_addrlen = sizeof(struct sockaddr_in);
	addr->ai_family = AF_INET;

	if ((ipaddr = inet_addr(node)) == INADDR_NONE) {
		if ((host = tds_gethostbyname_r(node, &result, buffer, sizeof(buffer), &h_errnop)) == NULL)
			goto Cleanup;
		if (host->h_name)
			addr->ai_canonname = strdup(host->h_name);
		ipaddr = *(in_addr_t *) host->h_addr;
	}

	if (service) {
		port = atoi(service);
		if (!port)
			port = tds_getservice(service);
	}

	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	sin->sin_port = htons(port);

	*res = addr;
	return 0;

Cleanup:
	if (addr != NULL)
		tds_freeaddrinfo(addr);

	return -1;
}

/* Incomplete implementation, ipv4 only, port does not work, flags do not work */
int
tds_getnameinfo(const struct sockaddr *sa, size_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	if (sa->sa_family != AF_INET)
		return EAI_FAMILY;

	if (host == NULL || hostlen < 16)
		return EAI_OVERFLOW;

#if defined(AF_INET) && HAVE_INET_NTOP
	inet_ntop(AF_INET, &sin->sin_addr, host, hostlen);
#elif HAVE_INET_NTOA_R
	inet_ntoa_r(sin->sin_addr, host, hostlen);
#else
	strlcpy(hostip, inet_ntoa(sin->sin_addr), hostlen);
#endif

	return 0;
}

void
tds_freeaddrinfo(struct tds_addrinfo *addr)
{
	assert(addr != NULL);
	free(addr->ai_canonname);
	free(addr->ai_addr);
	free(addr);
}

#endif
