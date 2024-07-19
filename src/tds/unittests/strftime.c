/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2020 Frediano Ziglio
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
 * Purpose: test tds_strftime.
 * This is a wrapper to strftime for portability and extension.
 */
#include "common.h"
#include <assert.h>
#include <freetds/convert.h>
#include <freetds/time.h>

static void
test(const TDSDATEREC* dr, int prec, const char *fmt, const char *expected, int line)
{
	char out[256];
	char *format = strdup(fmt);
	assert(format != NULL);

	tds_strftime(out, sizeof(out), format, dr, prec);

	if (strcmp(out, expected) != 0) {
		fprintf(stderr, "%d: Wrong results got '%s' expected '%s'\n", line, out, expected);
		exit(1);
	}

	free(format);
}

TEST_MAIN()
{
	TDSDATEREC dr;

	memset(&dr, 0, sizeof(dr));

#define TEST(prec, fmt, exp) test(&dr, prec, fmt, exp, __LINE__)

	/* %z extension, second decimals */
	TEST(3, "%z", "000");
	TEST(0, "x%z", "x");
	TEST(3, ".%z", ".000");
	dr.decimicrosecond = 1234567;
	TEST(5, ".%z", ".12345");
	TEST(4, "%z", "1234");
	TEST(4, "%z%%", "1234%");
	TEST(4, "%z%H", "123400");

	/* not terminated format, should not overflow */
	TEST(3, "%", "%");

	/* not portable %l, we should handle it */
	TEST(0, "%l", "12");
	TEST(0, "%%%l", "%12");
	dr.hour = 16;
	TEST(0, "%l", " 4");

	/* not portable %e, we should handle it */
	dr.day = 23;
	TEST(0, "%e", "23");
	dr.day = 5;
	TEST(0, "x%e", "x 5");
	return 0;
}
