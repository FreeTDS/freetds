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

#undef NDEBUG
#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include "replacements.h"

int main(void)
{
	char *buf = (char *) malloc(10);

	/* test tds_strlcpy */
	memset(buf, 0xff, 10);
	tds_strlcpy(buf, "test", 10);
	assert(strcmp(buf, "test") == 0);

	memset(buf, 0xff, 10);
	tds_strlcpy(buf, "TESTTEST", 10);
	assert(strcmp(buf, "TESTTEST") == 0);

	memset(buf, 0xff, 10);
	tds_strlcpy(buf, "abcdefghi", 10);
	assert(strcmp(buf, "abcdefghi") == 0);

	memset(buf, 0xff, 10);
	tds_strlcpy(buf, "1234567890", 10);
	assert(strcmp(buf, "123456789") == 0);

	memset(buf, 0xff, 10);
	tds_strlcpy(buf, "xyzabc1234567890", 10);
	assert(strcmp(buf, "xyzabc123") == 0);

	/* test tds_strlcat */
	strcpy(buf, "xyz");
	tds_strlcat(buf, "test", 10);
	assert(strcmp(buf, "xyztest") == 0);

	strcpy(buf, "xyz");
	tds_strlcat(buf, "TESTAB", 10);
	assert(strcmp(buf, "xyzTESTAB") == 0);

	strcpy(buf, "xyz");
	tds_strlcat(buf, "TESTabc", 10);
	assert(strcmp(buf, "xyzTESTab") == 0);

	strcpy(buf, "xyz");
	tds_strlcat(buf, "123456789012345", 10);
	assert(strcmp(buf, "xyz123456") == 0);

	strcpy(buf, "123456789");
	tds_strlcat(buf, "test", 4);
	assert(strcmp(buf, "123456789") == 0);

	free(buf);
	return 0;
}

