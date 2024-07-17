/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2020  Frediano Ziglio
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

/* allows to use some internal functions */
#undef NDEBUG
#include "../query.c"

#include "common.h"

#include <freetds/data.h>

static void
test_generic(const char *s, int expected_pos, bool comment, int line)
{
	const char *next;
	size_t len, n;
	char *buf;

	tdsdump_log(TDS_DBG_FUNC, "test line %d\n", line);

	/* multi byte */
	if (comment)
		next = tds_skip_comment(s);
	else
		next = tds_skip_quoted(s);
	tdsdump_log(TDS_DBG_INFO1, "returned ptr %p diff %ld\n", next, (long int) (next - s));
	assert(next >= s);
	assert(next - s == expected_pos);

	/* ucs2/utf16 */
	len = strlen(s);
	buf = tds_new(char, len * 2); /* use malloc to help memory debuggers */
	for (n = 0; n < len; ++n) {
		buf[n*2] = s[n];
		buf[n*2 + 1] = 0;
	}
	s = buf;
	if (comment)
		next = tds_skip_comment_ucs2le(s, s + len*2);
	else
		next = tds_skip_quoted_ucs2le(s, s + len*2);
	tdsdump_log(TDS_DBG_INFO1, "returned ptr %p diff %ld\n", next, (long int) (next - s));
	assert(next >= s);
	assert((next - s) % 2 == 0);
	assert((next - s) / 2 == expected_pos);
	free(buf);
}

#define test_comment(s, e) test_generic(s, e, true, __LINE__)
#define test_quote(s, e) test_generic(s, e, false, __LINE__)

TEST_MAIN()
{
	tdsdump_open(tds_dir_getenv(TDS_DIR("TDSDUMP")));

	/* test comment skipping */
	test_comment("--", 2);
	test_comment("--  aa", 6);
	test_comment("--\nx", 3);
	test_comment("/*", 2);
	test_comment("/*/", 3);
	test_comment("/*     a", 8);
	test_comment("/**/v", 4);
	test_comment("/* */x", 5);

	/* test quoted strings */
	test_quote("''", 2);
	test_quote("'a'", 3);
	test_quote("''''", 4);
	test_quote("'a'''", 5);
	test_quote("'''a'", 5);
	test_quote("'' ", 2);
	test_quote("'a'x", 3);
	test_quote("'''' ", 4);
	test_quote("'a'''x", 5);
	test_quote("'''a' ", 5);

	/* test quoted identifiers */
	test_quote("[]", 2);
	test_quote("[a]", 3);
	test_quote("[]]]", 4);
	test_quote("[a]]]", 5);
	test_quote("[]]a]", 5);
	test_quote("[]x", 2);
	test_quote("[a] ", 3);
	test_quote("[]]]x", 4);
	test_quote("[a]]] ", 5);
	test_quote("[]]a]x", 5);
	test_quote("[[]", 3);
	test_quote("[[[]", 4);
	test_quote("[[x[]", 5);

	return 0;
}
