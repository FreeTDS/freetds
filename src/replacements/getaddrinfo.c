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
#include <assert.h>

#include "tds.h"
#include "tds_sysdep_private.h"
#include "replacements.h"

/* Incomplete implementation, single ipv4 addr, service does not work, hints do not work */
int
tds_getaddrinfo(const char *node, const char *service, const struct tds_addrinfo *hints, struct tds_addrinfo **res)
{
	int retcode = 0;
	struct tds_addrinfo *addr;
	struct sockaddr_in *sin, *s;
	struct hostent *host = NULL;
	unsigned int ipaddr;
	char buffer[4096];
	struct hostent result;
	int h_errnop;

	assert(node != NULL);

	if ((addr = (tds_addrinfo *) malloc(sizeof(struct tds_addrinfo))) != NULL)
		memset(addr, '\0', sizeof(struct tds_addrinfo));
	else
		retcode = -1;

	if ((sin = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in))) != NULL)
		memset(sin, '\0', sizeof(struct sockaddr_in));
	else
		retcode = -1;

	if (retcode == 0) {
		addr->ai_addr = (struct sockaddr *) sin;
		addr->ai_addrlen = sizeof(struct sockaddr_in);
		addr->ai_family = AF_INET;
		sin = NULL;

		if ((ipaddr = inet_addr(node)) == INADDR_NONE) {
			if ((host = tds_gethostbyname_r(node, &result, buffer, sizeof(buffer), &h_errnop)) != NULL)
				ipaddr = *(unsigned int *) host->h_addr;
			else
				retcode = -1;
		}
	} else
		retcode = -1;

	if (retcode == 0) {
		s = (struct sockaddr_in *) addr->ai_addr;
		s->sin_family = AF_INET;
		s->sin_addr.s_addr = ipaddr;
		s->sin_port = htons(atoi(service));

		*res = addr;
		addr = NULL;
	}

	if (addr != NULL)
		tds_freeaddrinfo(addr);

	if (sin != NULL)
		free(sin);

	return retcode;
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
	tds_strlcpy(hostip, inet_ntoa(sin->sin_addr), hostlen);
#endif

	return 0;
}

void
tds_freeaddrinfo(struct tds_addrinfo *addr)
{
	assert(addr != NULL);
	if (addr->ai_addr != NULL)
		free(addr->ai_addr);
	free(addr);
}

#endif
