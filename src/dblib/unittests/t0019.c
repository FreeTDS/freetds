/* 
 * Purpose: Test Some conversion, check trimming error and results
 * Functions: dbconvert dberrhandle dbmsghandle dbinit dbexit
 */

#include "common.h"
#include <ctype.h>

#include <freetds/bool.h>

static bool failed = false;

static const char *cur_result = "";
static const char *cur_test = "";
static int cur_line = 0;

static int
err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	/*
	 * For server messages, cancel the query and rely on the
	 * message handler to spew the appropriate error messages out.
	 */
	if (dberr == SYBESMSG)
		return INT_CANCEL;

	if (dberr == 20049) {
		fprintf(stderr, "OK: anticipated error %d (%s) arrived\n", dberr, dberrstr);
	} else {
	fprintf(stderr,
		"DB-LIBRARY error (severity %d, dberr %d, oserr %d, dberrstr %s, oserrstr %s):\n",
		severity, dberr, oserr, dberrstr ? dberrstr : "(null)", oserrstr ? oserrstr : "(null)");
	}
	fflush(stderr);

	return INT_CANCEL;
}

static int
test(int srctype, const void *srcdata, int srclen, int dsttype, int dstlen)
{
	DBCHAR buf[10];
	char s[20], *p;
	int i, len, correct;
	char out[256];

	memset(buf, '*', sizeof(buf));
	len = dbconvert(NULL, srctype, (const BYTE*) srcdata, srclen, dsttype, (BYTE*) buf, dstlen);

	/* build result string */
	sprintf(out, "len=%d", len);
	p = s;
	for (i = 0; i < sizeof(buf); ++i) {
		*p++ = isprint((unsigned char) buf[i]) ? buf[i] : '.';
		sprintf(strchr(out, 0), " %02X", (unsigned char) buf[i]);
	}
	*p = 0;

	correct = 0;
	if (len == -1) {
		if (strcmp(cur_result, "error") == 0)
			correct = 1;
	} else {
		if (strcmp(cur_result, out) == 0)
			correct = 1;
	}
	if (!correct) {
		failed = true;
		printf("\nline: %d test: %s\n" "%s\n%s\n" "failed :( should be '%s'\n", cur_line, cur_test, s, out, cur_result);
	}
	return 0;
}

#define TEST(s,out) \
	{ cur_result = out; cur_line = __LINE__; cur_test = #s; test s; }

TEST_MAIN()
{
	if (dbinit() == FAIL)
		return 1;

	dberrhandle(err_handler);
	dbmsghandle(syb_msg_handler);

	TEST((SYBBINARY, "ciao\0\0", 6, SYBBINARY, -2), "len=6 63 69 61 6F 00 00 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, -2), "len=6 63 69 61 6F 20 20 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, -2), "len=6 63 69 61 6F 00 00 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, -1), "len=4 63 69 61 6F 00 2A 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, -1), "len=6 63 69 61 6F 00 00 00 2A 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 8), "len=6 63 69 61 6F 20 20 20 20 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 8), "len=6 63 69 61 6F 00 00 20 20 2A 2A");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 4), "error");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 4), "error");
	TEST((SYBCHAR, "ciao  ", 6, SYBCHAR, 6), "len=6 63 69 61 6F 20 20 2A 2A 2A 2A");
	TEST((SYBCHAR, "ciao\0\0", 6, SYBCHAR, 6), "len=6 63 69 61 6F 00 00 2A 2A 2A 2A");

	/* convert from NULL to BINARY */
	TEST((SYBBINARY,    "", 0, SYBBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBVARBINARY, "", 0, SYBBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBIMAGE,     "", 0, SYBBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBBINARY,    "", 0, SYBVARBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBVARBINARY, "", 0, SYBVARBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBIMAGE,     "", 0, SYBVARBINARY, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBBINARY,    "", 0, SYBIMAGE, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBVARBINARY, "", 0, SYBIMAGE, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");
	TEST((SYBIMAGE,     "", 0, SYBIMAGE, 6), "len=6 00 00 00 00 00 00 2A 2A 2A 2A");

	TEST((SYBBINARY, "", 0, SYBBINARY, -1), "len=0 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A");

	dbexit();
	if (!failed)
		printf("All tests passed!\n");
	return failed ? 1 : 0;
}
