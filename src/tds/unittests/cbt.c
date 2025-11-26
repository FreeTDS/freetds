/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2025  Frediano Ziglio
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
 * Purpose: test channel binding code
 */
#undef NDEBUG
#include "../challenge.c"

#include "common.h"

static char *
bin2ascii(char *dest, const void *data, size_t len)
{
	char *s = dest;
	const unsigned char *src = (const unsigned char *) data;

	for (; len > 0; --len, s += 2)
		sprintf(s, "%02x", *src++);
	*s = 0;
	return dest;
}

static void
calc_cbt_from_tls_unique_test(const char *tls_unique, const char *cbt)
{
	unsigned char cbt_buf[16];
	char cbt_str[33];

	tds_calc_cbt_from_tls_unique(tls_unique, strlen(tls_unique), cbt_buf);

	/* convert to hex string and compare */
	bin2ascii(cbt_str, cbt_buf, 16);
	if (strcasecmp(cbt_str, cbt) != 0) {
		fprintf(stderr, "Wrong calc_cbt_from_tls_unique(%s) -> %s expected %s\n", tls_unique, cbt_buf, cbt);
		exit(1);
	}
}

TEST_MAIN()
{
	calc_cbt_from_tls_unique_test("0123456789abcdef", "1eb7620a5e38cb1f50478b1690621a03");
	return 0;
}
