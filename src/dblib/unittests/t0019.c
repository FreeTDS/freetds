/*
 * ** t0019.c: Some conversion, check trimming error and results
 * **
 * */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#define DBNTWIN32
#include <windows.h>
#endif
#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static failure = 0;

static const char *cur_result = "";
static const char *cur_test = "";
static int cur_line = 0;

int test(int srctype,void *srcdata,int srclen,int dsttype,int dstlen) {
	DBCHAR	buf[10];
	char	s[20],*p;
	int	i,len,correct;
	char	out[256];

	memset(buf,'*',sizeof(buf));
	len = dbconvert(NULL, srctype, srcdata, srclen, dsttype, buf, dstlen);

	/* build result string */
	sprintf(out,"len=%d",len);
	p = s;
	for(i=0; i<sizeof(buf); ++i) {
		*p++ = isprint(buf[i])?buf[i]:'.';
		sprintf(strchr(out,0)," %02X",(unsigned char)buf[i]);
	}
	*p = 0;

	correct = 0;
	if (len == -1 || len ==FAIL) {
		if (strcmp(cur_result,"error")==0) correct = 1;
	} else {
		if (strcmp(cur_result,out)==0) correct = 1;
	}
	if (!correct) {
		failure = 1;
		printf("\nline: %d test: %s\n"
				"%s\n%s\n"
				"failed :( should be '%s'\n",
				cur_line,cur_test,
				s,out,
				cur_result);
	}
	return 0;
}

#define TEST(s,out) \
	{ cur_result = out; cur_line = __LINE__; cur_test = #s; test s; }

int main(int argc, char *argv[])
{
	if (dbinit() == FAIL)
		return 1;

	dberrhandle( syb_err_handler );
	dbmsghandle( syb_msg_handler );

	TEST((SYBBINARY, "ciao\0\0", 6, SYBBINARY, -2),
			"len=6 63 69 61 6F 00 00 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, -2),
			"len=6 63 69 61 6F 20 20 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, -2),
			"len=6 63 69 61 6F 00 00 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, -1),
			"len=4 63 69 61 6F 00 2A 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, -1),
			"len=6 63 69 61 6F 00 00 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 8),
			"len=6 63 69 61 6F 20 20 20 20 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 8),
			"len=6 63 69 61 6F 00 00 20 20 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 4),
			"error");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 4),
			"error");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 6),
			"len=6 63 69 61 6F 20 20 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 6),
			"len=6 63 69 61 6F 00 00 2A 2A 2A 2A");

	dbexit();
	if (!failure) printf ("All tests passed!\n");
	return failure;
}

