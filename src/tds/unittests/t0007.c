/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#include "common.h"
#include <tdsconvert.h>

static char software_version[] = "$Id: t0007.c,v 1.13.2.1 2006-06-08 08:19:32 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static TDSCONTEXT ctx;

void test0(const char *src, int len, int dsttype, const char *result);
void test(const char *src, int dsttype, const char *result);

void
test0(const char *src, int len, int dsttype, const char *result)
{
	int i, res;
	char buf[256];
	CONV_RESULT cr;

	res = tds_convert(&ctx, SYBVARCHAR, src, len, dsttype, &cr);
	if (res < 0)
		strcpy(buf, "error");
	else {
		switch (dsttype) {
		case SYBINT1:
			sprintf(buf, "%d", cr.ti);
			break;
		case SYBINT2:
			sprintf(buf, "%d", cr.si);
			break;
		case SYBINT4:
			sprintf(buf, "%d", cr.i);
			break;
		case SYBUNIQUE:
			sprintf(buf, "%08X-%04X-%04X-%02X%02X%02X%02X"
				"%02X%02X%02X%02X",
				cr.u.Data1,
				cr.u.Data2, cr.u.Data3,
				cr.u.Data4[0], cr.u.Data4[1],
				cr.u.Data4[2], cr.u.Data4[3], cr.u.Data4[4], cr.u.Data4[5], cr.u.Data4[6], cr.u.Data4[7]);
			break;
		case SYBBINARY:
			sprintf(buf, "len=%d", res);
			for (i = 0; i < res; ++i)
				sprintf(strchr(buf, 0), " %02X", (TDS_UCHAR) cr.ib[i]);
			free(cr.ib);
			break;
		case SYBDATETIME:
			sprintf(buf, "%ld %ld", (long int) cr.dt.dtdays, (long int) cr.dt.dttime);
			break;
		}
	}
	printf("%s\n", buf);
	if (strcmp(buf, result) != 0) {
		fprintf(stderr, "Expected %s\n", result);
		exit(1);
	}
}

void
test(const char *src, int dsttype, const char *result)
{
	test0(src, strlen(src), dsttype, result);
}

int
main(int argc, char **argv)
{
	memset(&ctx, 0, sizeof(ctx));

	/* test some conversion */
	printf("some checks...\n");
	test("1234", SYBINT4, "1234");
	test("123", SYBINT1, "123");
	test("  -    1234   ", SYBINT2, "-1234");
	test("  -    1234   a", SYBINT2, "error");

	/* test for overflow */
	printf("overflow checks...\n");
	test("2147483647", SYBINT4, "2147483647");
	test("2147483648", SYBINT4, "error");
	test("-2147483648", SYBINT4, "-2147483648");
	test("-2147483649", SYBINT4, "error");
	test("32767", SYBINT2, "32767");
	test("32768", SYBINT2, "error");
	test("-32768", SYBINT2, "-32768");
	test("-32769", SYBINT2, "error");
	test("255", SYBINT1, "255");
	test("256", SYBINT1, "error");
	test("0", SYBINT1, "0");
	test("-1", SYBINT1, "error");

	/*
	 * test overflow on very big numbers 
	 * i use increment of 10^9 to be sure lower 32bit be correct
	 * in a case
	 */
	printf("overflow on big number checks...\n");
	test("62147483647", SYBINT4, "error");
	test("63147483647", SYBINT4, "error");
	test("64147483647", SYBINT4, "error");
	test("65147483647", SYBINT4, "error");

	/* test not terminated string */
	test0("1234", 2, SYBINT4, "12");

	/* some test for unique */
	printf("unique type...\n");
	test("12345678-1234-1234-9876543298765432", SYBUNIQUE, "12345678-1234-1234-9876543298765432");
	test("{12345678-1234-1E34-9876ab3298765432}", SYBUNIQUE, "12345678-1234-1E34-9876AB3298765432");
	test(" 12345678-1234-1234-9876543298765432", SYBUNIQUE, "error");
	test(" {12345678-1234-1234-9876543298765432}", SYBUNIQUE, "error");
	test("12345678-1234-G234-9876543298765432", SYBUNIQUE, "error");
	test("12345678-1234-a234-9876543298765432", SYBUNIQUE, "12345678-1234-A234-9876543298765432");
	test("123a5678-1234-a234-98765-43298765432", SYBUNIQUE, "error");
	test("123-5678-1234-a234-9876543298765432", SYBUNIQUE, "error");

	printf("binary test...\n");
	test("0x1234", SYBBINARY, "len=2 12 34");
	test("0xaBFd  ", SYBBINARY, "len=2 AB FD");
	test("AbfD  ", SYBBINARY, "len=2 AB FD");
	test("0x000", SYBBINARY, "len=2 00 00");
	test("0x0", SYBBINARY, "len=1 00");
	test("0x100", SYBBINARY, "len=2 01 00");
	test("0x1", SYBBINARY, "len=1 01");

	test("Jan 01 2006", SYBDATETIME, "38716 0");
	test("January 01 2006", SYBDATETIME, "38716 0");
	test("March 05 2005", SYBDATETIME, "38414 0");
	test("may 13 2001", SYBDATETIME, "37022 0");

	test("02 Jan 2006", SYBDATETIME, "38717 0");
	test("2 Jan 2006", SYBDATETIME, "38717 0");
	test("02Jan2006", SYBDATETIME, "38717 0");
	test("20060102", SYBDATETIME, "38717 0");
	test("060102", SYBDATETIME, "38717 0");

	return 0;
}
