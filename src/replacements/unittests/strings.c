/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2014  Frediano Ziglio
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

#include <freetds/utils/test_base.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include <freetds/replacements.h>

/* If the system supplies these, we're going to simulate the situation
 * where it doesn't so we're always testing our own versions.
 */
#if HAVE_STRLCPY
size_t tds_strlcpy(char *dest, const char *src, size_t len);
#include "../strlcpy.c"
#endif

#if HAVE_STRLCAT
size_t tds_strlcat(char *dest, const char *src, size_t len);
#include "../strlcat.c"
#endif

TEST_MAIN()
{
	char *buf = (char *) malloc(10);

	/* test tds_strlcpy */
	memset(buf, 0xff, 10);
	assert(tds_strlcpy(buf, "test", 10) == 4);
	assert(strcmp(buf, "test") == 0);

	memset(buf, 0xff, 10);
	assert(tds_strlcpy(buf, "TESTTEST", 10) == 8);
	assert(strcmp(buf, "TESTTEST") == 0);

	memset(buf, 0xff, 10);
	assert(tds_strlcpy(buf, "abcdefghi", 10) == 9);
	assert(strcmp(buf, "abcdefghi") == 0);

	memset(buf, 0xff, 10);
	assert(tds_strlcpy(buf, "1234567890", 10) == 10);
	assert(strcmp(buf, "123456789") == 0);

	memset(buf, 0xff, 10);
	assert(tds_strlcpy(buf, "xyzabc1234567890", 10) == 16);
	assert(strcmp(buf, "xyzabc123") == 0);

	/* test tds_strlcat */
	strcpy(buf, "xyz");
	assert(tds_strlcat(buf, "test", 10) == 7);
	assert(strcmp(buf, "xyztest") == 0);

	strcpy(buf, "xyz");
	assert(tds_strlcat(buf, "TESTAB", 10) == 9);
	assert(strcmp(buf, "xyzTESTAB") == 0);

	strcpy(buf, "xyz");
	assert(tds_strlcat(buf, "TESTabc", 10) == 10);
	assert(strcmp(buf, "xyzTESTab") == 0);

	strcpy(buf, "xyz");
	assert(tds_strlcat(buf, "123456789012345", 10) == 18);
	assert(strcmp(buf, "xyz123456") == 0);

	strcpy(buf, "123456789");
	assert(tds_strlcat(buf, "test", 4) == 13);
	assert(strcmp(buf, "123456789") == 0);

	/* test length == 0 */
	assert(tds_strlcpy(buf + 10, "test", 0) == 4);

	strcpy(buf, "123");
	assert(tds_strlcat(buf, "456", 0) == 6);
	assert(strcmp(buf, "123") == 0);

	free(buf);
	return 0;
}

