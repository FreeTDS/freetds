/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2010-2018  Frediano Ziglio
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

#define TDS_INTERNAL_TEST 1

#include <freetds/utils/test_base.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/replacements.h>

static void
test(const char *s, const char *sep)
{
	size_t len = strlen(s);
	char *c1 = (char*) malloc(len+1);
	char *c2 = (char*) malloc(len+1);
	char *last = NULL, *s1, *s2;
	const char *p1, *p2;

	printf("testint '%s' with '%s' separator(s)\n", s, sep);
	strcpy(c1, s);
	strcpy(c2, s);

	s1 = c1;
	s2 = c2;
	for (;;) {
		p1 = strtok(s1, sep);
		p2 = strtok_r(s2, sep, &last);
		s1 = s2 = NULL;
		if ((p1 && !p2) || (!p1 && p2)) {
			fprintf(stderr, "ptr mistmach %p %p\n", p1, p2);
			exit(1);
		}
		if (!p1)
			break;
		if (strcmp(p1, p2) != 0) {
			fprintf(stderr, "string mistmach '%s' '%s'\n", p1, p2);
			exit(1);
		}
		printf("got string %s\n", p1);
	}
	printf("\n");
	free(c1);
	free(c2);
}

TEST_MAIN()
{
	test("a b\tc", "\t ");
	test("    x  y \t  z", " \t");
	test("a;b;c;", ";");
	test("a;b;  c;;", ";");
	test("", ";");
	test(";;;", ";");
	return 0;
}
