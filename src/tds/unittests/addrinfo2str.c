/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026 Frediano Ziglio
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

/*
 * Test tds_addrinfo2str function.
 */
#include "common.h"

static int res = EXIT_SUCCESS;

static void
fill_bytes(void *buf, size_t len, uint8_t start)
{
	uint8_t *p = (uint8_t *) buf;

	for (; len; --len)
		*p++ = start++;
}

static void
check(struct addrinfo *ai, const char *expected, int line)
{
	char buf[128];
	const char *got;

	memset(buf, 0xab, sizeof(buf));
	got = tds_addrinfo2str(ai, buf, sizeof(buf));
	if (strcmp(got, expected) == 0)
		return;

	fprintf(stderr, "%d: Wrong result, got '%s' expected '%s'\n", line, got, expected);
	res = EXIT_FAILURE;
}

#define check(ai, exp) check(ai, exp, __LINE__)

TEST_MAIN()
{
	struct addrinfo ai;
	struct sockaddr_in sa4;

#ifdef AF_INET6
	struct sockaddr_in6 sa6;
#endif

	tds_socket_init();

	memset(&ai, 0, sizeof(ai));

	ai.ai_socktype = SOCK_STREAM;

	/* IPv4 */
	ai.ai_family = AF_INET;
	ai.ai_addr = (struct sockaddr *) &sa4;
	ai.ai_addrlen = sizeof(sa4);
	memset(&sa4, 0xcd, sizeof(sa4));
	sa4.sin_family = AF_INET;
	fill_bytes(&sa4.sin_addr, 4, 1);
	check(&ai, "1.2.3.4");

	/* IPv6 */
#ifdef AF_INET6
	ai.ai_family = AF_INET6;
	ai.ai_addr = (struct sockaddr *) &sa6;
	ai.ai_addrlen = sizeof(sa6);
	memset(&sa6, 0xef, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	fill_bytes(&sa6.sin6_addr, 16, 0x12);
	sa6.sin6_scope_id = 0;
	check(&ai, "1213:1415:1617:1819:1a1b:1c1d:1e1f:2021");

	sa6.sin6_scope_id = 12345678;
	check(&ai, "1213:1415:1617:1819:1a1b:1c1d:1e1f:2021%12345678");
#endif

	return res;
}
