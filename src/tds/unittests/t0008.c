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

static char  software_version[]   = "$Id: t0008.c,v 1.1 2002-08-16 11:00:19 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

extern int g__numeric_bytes_per_prec[39];

int g_result = 0;

void test(const char* test, const char* result,int prec,int scale)
{
	int i;
	char buf[256];
	CONV_RESULT cr;

	memset(&cr.n,0,sizeof(cr.n));
	cr.n.precision = prec;
	cr.n.scale = scale;
	if (tds_convert(NULL,SYBVARCHAR,test,strlen(test),SYBNUMERIC,sizeof(cr.n),&cr) == TDS_FAIL)
		strcpy(buf,"error");
	else
	{
		sprintf(buf,"prec=%d scale=%d",cr.n.precision,cr.n.scale);
		for(i=0; i < sizeof(cr.n.array); ++i)
			sprintf(strchr(buf,0)," %02X",cr.n.array[i]);
	}
	printf("%s\n",buf);
	if (strcmp(buf,result)!=0)
	{
		fprintf(stderr,"Failed! Should be\n%s\n",result);
		g_result = 1;
	}
}

int main()
{
	/* very long string for test buffer overflow */
	int i;
	char long_test[201];

	g__numeric_bytes_per_prec[ 0] = -1;
	g__numeric_bytes_per_prec[ 1] = 2;
	g__numeric_bytes_per_prec[ 2] = 2;
	g__numeric_bytes_per_prec[ 3] = 3;
	g__numeric_bytes_per_prec[ 4] = 3;
	g__numeric_bytes_per_prec[ 5] = 4;
	g__numeric_bytes_per_prec[ 6] = 4;
	g__numeric_bytes_per_prec[ 7] = 4;
	g__numeric_bytes_per_prec[ 8] = 5;
	g__numeric_bytes_per_prec[ 9] = 5;
	g__numeric_bytes_per_prec[10] = 6;
	g__numeric_bytes_per_prec[11] = 6;
	g__numeric_bytes_per_prec[12] = 6;
	g__numeric_bytes_per_prec[13] = 7;
	g__numeric_bytes_per_prec[14] = 7;
	g__numeric_bytes_per_prec[15] = 8;
	g__numeric_bytes_per_prec[16] = 8;
	g__numeric_bytes_per_prec[17] = 9;
	g__numeric_bytes_per_prec[18] = 9;
	g__numeric_bytes_per_prec[19] = 9;
	g__numeric_bytes_per_prec[20] = 10;
	g__numeric_bytes_per_prec[21] = 10;
	g__numeric_bytes_per_prec[22] = 11;
	g__numeric_bytes_per_prec[23] = 11;
	g__numeric_bytes_per_prec[24] = 11;
	g__numeric_bytes_per_prec[25] = 12;
	g__numeric_bytes_per_prec[26] = 12;
	g__numeric_bytes_per_prec[27] = 13;
	g__numeric_bytes_per_prec[28] = 13;
	g__numeric_bytes_per_prec[29] = 14;
	g__numeric_bytes_per_prec[30] = 14;
	g__numeric_bytes_per_prec[31] = 14;
	g__numeric_bytes_per_prec[32] = 15;
	g__numeric_bytes_per_prec[33] = 15;
	g__numeric_bytes_per_prec[34] = 16;
	g__numeric_bytes_per_prec[35] = 16;
	g__numeric_bytes_per_prec[36] = 16;
	g__numeric_bytes_per_prec[37] = 17;
	g__numeric_bytes_per_prec[38] = 17;

	printf("test some valid values..\n");
	test("    1234",      "prec=18 scale=0 00 00 00 00 00 00 00 04 D2 00 00 00 00 00 00 00 00",18,0);
	test("1234567890","prec=18 scale=0 00 00 00 00 00 49 96 02 D2 00 00 00 00 00 00 00 00",18,0);
	test("123456789012345678","prec=18 scale=0 00 01 B6 9B 4B A6 30 F3 4E 00 00 00 00 00 00 00 00",18,0);
	test("999999999999999999","prec=18 scale=0 00 0D E0 B6 B3 A7 63 FF FF 00 00 00 00 00 00 00 00",18,0);
	
	printf("test overflow..\n");
	test("123456789012345678901234567890","error",18,0);

	long_test[0] = 0;
	for(i=0;i < 20; ++i)
		strcat(long_test,"1234567890");
	test(long_test,"error",18,0);

	/* return only precision 18 scale 0
	test("123456789012345678901234567890","prec=38 scale=0 00 D2 0A 3F 4E EE E0 73 C3 F6 0F E9 8E 01 00 00 00",38,0);
	test("1234567890123456789012345678901234567890123456789012345678901234567890","error",38,0);
	test("99999999999999999999999999999999999999","prec=38 scale=0 00 FF FF FF FF 3F 22 8A 09 7A C4 86 5A A8 4C 3B 4B",38,0);
	test("100000000000000000000000000000000000000","error",38,0);
	*/

	return g_result;
}
