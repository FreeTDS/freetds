/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026  Frediano Ziglio
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
 * Purpose: test NTLM code (user name upper case conversion).
 */
#undef NDEBUG
#include "../challenge.c"

#include "common.h"
#include <assert.h>

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
check(const char *name, const void *data, size_t len, const char *expected)
{
	char buf[1024];

	assert(len * 2 < sizeof(buf));
	bin2ascii(buf, data, len);
	if (strcmp(buf, expected) != 0) {
		fprintf(stderr, "Wrong %s: got %s expected %s\n", name, buf, expected);
		exit(1);
	}
}

static void
test_upper(void)
{
	static const uint16_t tests[][2] = {
		{0x0061, 0x0041},	/* a */
		{0x007a, 0x005a},	/* z */
		{0x0041, 0x0041},	/* A */
		{0x00e9, 0x00c9},	/* e acute */
		{0x00ff, 0x0178},	/* y diaeresis */
		{0x00df, 0x00df},	/* sharp s, no simple mapping */
		{0x0131, 0x0049},	/* dotless i */
		{0x0101, 0x0100},	/* a macron, delta 0xffff range, every other character */
		{0x0100, 0x0100},	/* A macron */
		{0x03c3, 0x03a3},	/* sigma */
		{0x03c2, 0x03a3},	/* final sigma */
		{0x044f, 0x042f},	/* cyrillic ya */
		{0x0451, 0x0401},	/* cyrillic io */
		{0x1f80, 0x1f88},	/* greek alpha with psili and ypogegrammeni */
		{0x1f51, 0x1f59},	/* greek upsilon with dasia, delta 8 every other character */
		{0x1f52, 0x1f52},	/* greek upsilon with psili and varia, no simple mapping */
		{0x1f57, 0x1f5f},	/* greek upsilon with dasia and perispomeni */
		{0xff41, 0xff21},	/* fullwidth a */
		{0xd800, 0xd800},	/* surrogate */
		{0x20ac, 0x20ac},	/* euro sign */
	};
	unsigned int i;
	char buf[4];

	for (i = 0; i < TDS_VECTOR_SIZE(tests); ++i) {
		if (ucs2_upper(tests[i][0]) != tests[i][1]) {
			fprintf(stderr, "Wrong upper case of %04x: got %04x expected %04x\n",
				tests[i][0], ucs2_upper(tests[i][0]), tests[i][1]);
			exit(1);
		}
	}

	/* string conversion, UTF-16LE */
	memcpy(buf, "\xe9\x00\x3c\x04", 4);
	ucs2le_to_upper(buf, sizeof(buf));
	check("UTF-16 upper", buf, sizeof(buf), "c9001c04");
}

TEST_MAIN()
{
	test_upper();
	return 0;
}
