/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2019  Frediano Ziglio
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
 * Purpose: test bytes.h header.
 */

#include <freetds/utils/test_base.h>

#include <stdio.h>
#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "tds_sysdep_public.h"
#include <freetds/bytes.h>

#define WRITE(buf, off, bytes, endian, val) do { \
	if ((off % bytes) == 0) TDS_PUT_A ## bytes ## endian(buf+off, val); \
	else TDS_PUT_UA ## bytes ## endian(buf+off, val); \
} while(0)

#define READ(buf, off, bytes, endian) ( \
	((off % bytes) == 0) ? TDS_GET_A ## bytes ## endian(buf+off) : \
	TDS_GET_UA ## bytes ## endian(buf+off) \
)

/* check read and write works at a given offset */
#define CHECK(off, bytes, endian, expected) do { \
	uint32_t val = READ(bufs.u1.buf, off, bytes, endian); \
	WRITE(bufs.u2.buf, off, bytes, endian, val); \
	assert(READ(bufs.u1.buf, off, bytes, endian) == READ(bufs.u2.buf, off, bytes, endian)); \
	assert(val == expected); \
} while(0)

TEST_MAIN()
{
	/* this structure make sure buffer are properly aligned */
	struct {
		union {
			uint8_t buf[26];
			uint32_t a1;
		} u1;
		union {
			uint8_t buf[26];
			uint32_t a2;
		} u2;
	} bufs;
	unsigned n;

	memset(&bufs, 0, sizeof(bufs));
	for (n = 0; n < sizeof(bufs.u1.buf); ++n)
		bufs.u1.buf[n] = (uint8_t) (123 * n + 67);

	/* aligned access */
	CHECK(0, 4, LE, 0xb439be43u);
	CHECK(4, 2, LE, 0xaa2fu);
	CHECK(6, 2, BE, 0x25a0u);
	CHECK(8, 4, BE, 0x1b96118cu);

	CHECK(12, 1, LE, 0x7u);

	/* unaligned access */
	CHECK(13, 2, BE, 0x82fdu);
	CHECK(15, 2, LE, 0xf378u);
	CHECK(17, 4, LE, 0xdf64e96eu);
	CHECK(21, 4, BE, 0x5ad550cbu);

	CHECK(25, 1, BE, 0x46);
	assert(memcmp(bufs.u1.buf, bufs.u2.buf, sizeof(bufs.u1.buf)) == 0);

	return 0;
}

