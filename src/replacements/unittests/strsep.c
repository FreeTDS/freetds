/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2018  Frediano Ziglio
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

#undef NDEBUG
#include <config.h>

#ifdef HAVE_STRSEP
char *tds_strsep(char **stringp, const char *delim);
#include "../strsep.c"

#include <freetds/utils/test_base.h>

#else

#include <freetds/utils/test_base.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/replacements.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

/* test strsep with same separators */
static void
test(char *s, const char *sep, ...)
{
	char *copy = strdup(s);
	const char *out, *expected;
	va_list ap;

	printf("testing '%s' with '%s' separator(s)\n", s, sep);

	s = copy;
	va_start(ap, sep);
	do {
		out = tds_strsep(&s, sep);
		expected = va_arg(ap, const char *);
		if (expected) {
			assert(out && strcmp(out, expected) == 0);
		} else {
			assert(out == NULL);
		}
	} while (expected != NULL);
	va_end(ap);

	/* should continue to give NULL */
	assert(tds_strsep(&s, sep) == NULL);
	assert(tds_strsep(&s, sep) == NULL);

	free(copy);
}

/* test with different separators */
static void
test2(void)
{
	char buf[] = "one;two=value";
	char *s = buf;
	assert(strcmp(tds_strsep(&s, ";:"), "one") == 0);
	assert(strcmp(tds_strsep(&s, "="), "two") == 0);
	assert(strcmp(tds_strsep(&s, ""), "value") == 0);
	assert(tds_strsep(&s, "") == NULL);
}

TEST_MAIN()
{
	test("a b c", "", "a b c", NULL);
	test("a b c", " ", "a", "b", "c", NULL);
	test("a  c", " ", "a", "", "c", NULL);
	test("a b\tc", " \t", "a", "b", "c", NULL);
	test("a b\tc ", " \t", "a", "b", "c", "", NULL);
	test(" a b\tc", " \t", "", "a", "b", "c", NULL);
	test(",,", ",", "", "", "", NULL);
	test(",foo,", ",", "", "foo", "", NULL);
	test2();
	return 0;
}
