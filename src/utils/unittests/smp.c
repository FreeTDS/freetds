/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2022  Frediano Ziglio
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
 * Purpose: test smp library.
 */

#include <freetds/utils/test_base.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freetds/bool.h>
#include <freetds/utils/smp.h>

static void
same_smp(smp n, const char *s, int line)
{
	char *out = smp_to_string(n);
	if (strcmp(s, out) != 0) {
		fprintf(stderr, "%d: Wrong number, expected %s got %s\n", line, s, out);
		free(out);
		exit(1);
	}
	free(out);
}
#define same_smp(n, s) same_smp(n, s, __LINE__)

static void
same_int(int n, int expected, int line)
{
	if (n != expected) {
		fprintf(stderr, "%d: Wrong number, expected %d got %d\n", line, expected, n);
		exit(1);
	}
}
#define same_int(n, e) same_int(n, e, __LINE__)


TEST_MAIN()
{
	smp n;
	smp a;

	n = smp_from_int(1234567890123l);
	same_smp(n, "1234567890123");
	printf("%.20g\n", smp_to_double(n));

	n = smp_add(n, n);
	same_smp(n, "2469135780246");
	printf("%.20g\n", smp_to_double(n));

	printf("is_negative %d\n", smp_is_negative(n));
	same_int(smp_is_negative(n), false);

	n = smp_negate(n);
	same_smp(n, "-2469135780246");
	printf("%.20g\n", smp_to_double(n));

	printf("is_negative %d\n", smp_is_negative(n));
	same_int(smp_is_negative(n), true);

	n = smp_from_int(-87654321);
	same_smp(n, "-87654321");
	printf("%.20g\n", smp_to_double(n));

	a = smp_from_int(87654321);
	same_smp(a, "87654321");

	n = smp_add(n, a);
	same_smp(n, "0");
	printf("%.20g\n", smp_to_double(n));

	n = smp_sub(n, a);
	same_smp(n, "-87654321");
	printf("%.20g\n", smp_to_double(n));

	n = smp_from_int(4611686018427387904l);
	same_smp(n, "4611686018427387904");

	n = smp_add(n, n);
	same_smp(n, "9223372036854775808");

	n = smp_add(n, n);
	same_smp(n, "18446744073709551616");

	same_int(smp_cmp(smp_from_int(123), smp_from_int(123)), 0);
	same_int(smp_cmp(smp_from_int(123), smp_from_int(-124)), 1);
	same_int(smp_cmp(smp_from_int(-123), smp_from_int(123)), -1);
	return 0;
}

