#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <stdio.h>
#include <ctpublic.h>

static char software_version[] = "$Id: t0006.c,v 1.8 2002-11-20 13:57:15 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

CS_CONTEXT *ctx;
int allSuccess = 1;

typedef const char *STR;

int DoTest(TDS_INT fromtype, void *fromdata, TDS_INT fromlen,
	   TDS_INT totype, TDS_INT tomaxlen,
	   CS_RETCODE tores, void *todata, TDS_INT tolen,
	   STR sdecl,
	   STR sfromtype, STR sfromdata, STR sfromlen, STR stotype, STR stomaxlen, STR stores, STR stodata, STR stolen, int line);

int
DoTest(
	      /* source information */
	      TDS_INT fromtype, void *fromdata, TDS_INT fromlen,
	      /* to information */
	      TDS_INT totype, TDS_INT tomaxlen,
	      /* expected result */
	      CS_RETCODE tores, void *todata, TDS_INT tolen,
	      STR sdecl,
	      STR sfromtype, STR sfromdata, STR sfromlen, STR stotype, STR stomaxlen, STR stores, STR stodata, STR stolen, int line)
{
	CS_DATAFMT destfmt, srcfmt;
	CS_INT reslen;
	CS_RETCODE retcode;
	int i;
	char buffer[1024];
	const char *err = "";

	memset(&destfmt, 0, sizeof(destfmt));
	destfmt.datatype = totype;
	destfmt.maxlength = tomaxlen;

	memset(&srcfmt, 0, sizeof(srcfmt));
	srcfmt.datatype = fromtype;
	srcfmt.maxlength = fromlen;

	/* FIXME this fix some thing but if error cs_convert should return 
	 * CS_UNUSED; note that this is defined 5.. a valid result ... */
	reslen = 0;

	/* TODO: add special case for CS_CHAR_TYPE and give different 
	 * flags and len */

	/* do convert */
	memset(buffer, 23, sizeof(buffer));
	retcode = cs_convert(ctx, &srcfmt, fromdata, &destfmt, buffer, &reslen);

	/* test result of convert */
	if (tores != retcode) {
		err = "result";
		goto Failed;
	}

	/* test result len */
	if (reslen != tolen) {
		err = "result length";
		goto Failed;
	}

	/* test buffer */
	if (todata && memcmp(todata, buffer, tolen) != 0) {
		err = "result data";
		goto Failed;
	}

	/* test other part of buffer */
	if (todata)
		memset(buffer, 23, tolen);
	for (i = 0; i < sizeof(buffer); ++i)
		if (buffer[i] != 23) {
			err = "buffer left";
			goto Failed;
		}

	/* success */
	return 0;
      Failed:
	fprintf(stderr, "Test %s failed (ret=%d len=%d)\n", err, retcode, reslen);
	fprintf(stderr, "line: %d\n  DO_TEST(%s,\n"
		"\t   %s,%s,%s,\n"
		"\t   %s,%s,\n"
		"\t   %s,%s,%s);\n", line, sdecl, sfromtype, sfromdata, sfromlen, stotype, stomaxlen, stores, stodata, stolen);
	allSuccess = 0;
	return 1;
}

#define DO_TEST(decl,fromtype,fromdata,fromlen,totype,tomaxlen,tores,todata,tolen) { \
 decl; \
 DoTest(fromtype,fromdata,fromlen,totype,tomaxlen,tores,todata,tolen,\
  #decl,#fromtype,#fromdata,#fromlen,#totype,#tomaxlen,#tores,#todata,#tolen,\
  __LINE__);\
}

int
main(int argc, char **argv)
{
	CS_RETCODE ret;
	volatile TDS_INT8 one = 1;
	int verbose = 1;

	fprintf(stdout, "%s: Testing conversion\n", __FILE__);

	ret = cs_ctx_alloc(CS_VERSION_100, &ctx);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Init failed\n");
		return 1;
	}

	/* TODO For each conversion test different values of fromlen and tolen */

	/* 
	 * * INT to everybody 
	 */
	DO_TEST(CS_INT test = 12345;
		CS_INT test2 = 12345,
		CS_INT_TYPE, &test, sizeof(test), CS_INT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = 12345;
		CS_INT test2 = 12345,
		CS_INT_TYPE, &test, sizeof(test), CS_INT_TYPE, sizeof(test2) * 2, CS_SUCCEED, &test2, sizeof(test2));
	/* FIXME: correct ?? */
	DO_TEST(CS_INT test = 12345, CS_INT_TYPE, &test, sizeof(test), CS_INT_TYPE, sizeof(CS_SMALLINT), CS_FAIL, NULL, 0);

	DO_TEST(CS_INT test = 1234;
		CS_SMALLINT test2 = 1234, CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, 1, CS_SUCCEED, &test2, sizeof(test2));
	/* biggest and smallest SMALLINT */
	DO_TEST(CS_INT test = 32767;
		CS_SMALLINT test2 = 32767,
		CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -32768;
		CS_SMALLINT test2 = -32768,
		CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	/* overflow */
	DO_TEST(CS_INT test = 32768;
		CS_SMALLINT test2 = 12345, CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_INT test = -32769;
		CS_SMALLINT test2 = 12345, CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);

	/* biggest and smallest TINYINT */
	DO_TEST(CS_INT test = 255;
		CS_TINYINT test2 = 255,
		CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = 0;
		CS_TINYINT test2 = 0,
		CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	/* overflow */
	DO_TEST(CS_INT test = 256;
		CS_TINYINT test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_INT test = -1;
		CS_TINYINT test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);

	/* biggest and smallest BIT */
	DO_TEST(CS_INT test = 1;
		CS_BYTE test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_BIT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = 0;
		CS_BYTE test2 = 0, CS_INT_TYPE, &test, sizeof(test), CS_BIT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	/* overflow FIXME: or 1 if != 0 ?? */
	DO_TEST(CS_INT test = 2;
		CS_BYTE test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_BIT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -1;
		CS_BYTE test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_BIT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 1234;
		CS_REAL test2 = 1234.0,
		CS_INT_TYPE, &test, sizeof(test), CS_REAL_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -8765;
		CS_REAL test2 = -8765.0,
		CS_INT_TYPE, &test, sizeof(test), CS_REAL_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 1234;
		CS_FLOAT test2 = 1234.0,
		CS_INT_TYPE, &test, sizeof(test), CS_FLOAT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -8765;
		CS_FLOAT test2 = -8765.0,
		CS_INT_TYPE, &test, sizeof(test), CS_FLOAT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 1234678; CS_MONEY4 test2 = {
		1234678}
		, CS_INT_TYPE, &test, sizeof(test), CS_MONEY4_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_INT test = -8765; CS_MONEY4 test2 = {
		-8765 * 10000}
		, CS_INT_TYPE, &test, sizeof(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	/* strange money formatting */
	DO_TEST(CS_CHAR test[] = ""; CS_MONEY4 test2 = {
		0}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "."; CS_MONEY4 test2 = {
		0}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = ".12"; CS_MONEY4 test2 = {
		1200}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "++++-123"; CS_MONEY4 test2 = {
		-123 * 10000}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_CHAR test[] = "   -123"; CS_MONEY4 test2 = {
		-123 * 10000}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "   +123"; CS_MONEY4 test2 = {
		123 * 10000}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "+123.1234"; CS_MONEY4 test2 = {
		1231234}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "+123.123411"; CS_MONEY4 test2 = {
		1231234}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_CHAR test[] = "+123.12.3411"; CS_MONEY4 test2 = {
		1231234}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_CHAR test[] = "pippo"; CS_MONEY4 test2 = {
		0}
		, CS_CHAR_TYPE, test, strlen(test), CS_MONEY4_TYPE, sizeof(test2), CS_FAIL, NULL, 0);

	/* not terminated money  */
	DO_TEST(CS_CHAR test[] = "-1234567"; CS_MONEY4 test2 = {
		-1230000}
		, CS_CHAR_TYPE, test, 4, CS_MONEY4_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 1234678;
		CS_MONEY test2;
		test2.tdsoldmoney.mnyhigh = ((one * 1234678) * 10000) >> 32;
		test2.tdsoldmoney.mnylow = (TDS_UINT) ((one * 1234678) * 10000),
		CS_INT_TYPE, &test, sizeof(test), CS_MONEY_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -8765;
		CS_MONEY test2;
		test2.tdsoldmoney.mnyhigh = ((one * -8765) * 10000) >> 32;
		test2.tdsoldmoney.mnylow = (TDS_UINT) ((one * -8765) * 10000),
		CS_INT_TYPE, &test, sizeof(test), CS_MONEY_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 12345;
		CS_CHAR test2[] = "12345",
		CS_INT_TYPE, &test, sizeof(test), CS_CHAR_TYPE, sizeof(test2), CS_SUCCEED, test2, sizeof(test2) - 1);

/*
   DO_TEST(CS_INT test = 12345; CS_CHAR test2[] = "\005" "12345",
	   CS_INT_TYPE,&test,sizeof(test),
	   CS_VARBINARY_TYPE,sizeof(test2),
	   CS_SUCCEED,test2,sizeof(test2)-1);
*/

	DO_TEST(CS_CHAR test[] = "12345";
		CS_INT test2 = 12345, CS_CHAR_TYPE, test, 5, CS_INT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	/* unterminated number */
	DO_TEST(CS_CHAR test[] = " - 12345";
		CS_INT test2 = -12, CS_CHAR_TYPE, test, 5, CS_INT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	ret = cs_ctx_drop(ctx);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Drop failed\n");
		return 2;
	}

	if (verbose && allSuccess) {
		fprintf(stdout, "Test succeded\n");
	}
	return allSuccess ? 0 : 1;
}
