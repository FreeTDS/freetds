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

#include <stdio.h>
#include <tds.h>
#include <tdsconvert.h>

static char  software_version[]   = "$Id: t0007.c,v 1.1 2002-08-16 08:30:00 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

void test0(const char* test, int len, int dsttype, const char* result)
{
	int i;
	char buf[256];
	CONV_RESULT cr;

	if (tds_convert(NULL,SYBVARCHAR,test,len,dsttype,0,&cr) == TDS_FAIL)
		strcpy(buf,"error");
	else
	{
		switch(dsttype) {
			case SYBINT1:
				sprintf(buf,"%d",cr.ti);
				break;
			case SYBINT2:
				sprintf(buf,"%d",cr.si);
				break;
			case SYBINT4:
				sprintf(buf,"%d",cr.i);
				break;
		}
	}
	printf("%s\n",buf);
	if (strcmp(buf,result)!=0)
		exit(1);
}

void test(const char* test, int dsttype, const char* result)
{
	test0(test,strlen(test),dsttype,result);
}

int main()
{
	/* test some conversion */
	printf("some checks...\n");
	test("1234",SYBINT4,"1234");
	test("123",SYBINT1,"123");
	test("  -    1234   ",SYBINT2,"-1234");
	test("  -    1234   a",SYBINT2,"error");

	/* test for overflow */
	printf("overflow checks...\n");
	test("2147483647",SYBINT4,"2147483647");
	test("2147483648",SYBINT4,"error");
	test("-2147483648",SYBINT4,"-2147483648");
	test("-2147483649",SYBINT4,"error");
	test("32767",SYBINT2,"32767");
	test("32768",SYBINT2,"error");
	test("-32768",SYBINT2,"-32768");
	test("-32769",SYBINT2,"error");
	test("255",SYBINT1,"255");
	test("256",SYBINT1,"error");
	test("0",SYBINT1,"0");
	test("-1",SYBINT1,"error");

	/* test overflow on very big numbers 
	 * i use increment of 10^9 to be sure lower 32bit be correct
	 * in a case
	 * */
	printf("overflow on big number checks...\n");
	test("62147483647",SYBINT4,"error");
	test("63147483647",SYBINT4,"error");
	test("64147483647",SYBINT4,"error");
	test("65147483647",SYBINT4,"error");
	
	/* test not terminated string */
	test0("1234",2,SYBINT4,"12");

	return 0;
}
