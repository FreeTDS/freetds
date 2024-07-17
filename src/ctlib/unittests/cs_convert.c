#include "common.h"

#ifdef CS_TDS_74
#  define FREETDS 1
#else
#  define FREETDS 0
#endif

static CS_CONTEXT *ctx;
static int allSuccess = 1;

typedef const char *STR;

static CS_INT dest_format = CS_FMT_UNUSED;

static int
DoTest(
	      /* source information */
	      CS_INT fromtype, void *fromdata, size_t fromlen,
	      /* to information */
	      CS_INT totype, CS_INT tomaxlen,
	      /* expected result */
	      CS_RETCODE tores, void *todata, CS_INT tolen,
	      /* fields in string format */
	      STR sdecl,
	      STR sfromtype, STR sfromdata, STR sfromlen, STR stotype, STR stomaxlen, STR stores, STR stodata, STR stolen,
	      /* source line number for error reporting */
	      int line)
{
	CS_DATAFMT destfmt, srcfmt;
	CS_INT reslen;
	CS_RETCODE retcode;
	int i;
	char buffer[1024];
	const char *err = "";

	assert(tolen >= 0);

	memset(&destfmt, 0, sizeof(destfmt));
	destfmt.datatype = totype;
	destfmt.maxlength = tomaxlen;
	destfmt.format = dest_format;

	memset(&srcfmt, 0, sizeof(srcfmt));
	srcfmt.datatype = fromtype;
	srcfmt.maxlength = (CS_INT) fromlen;

	/*
	 * FIXME this fix some thing but if error cs_convert should return
	 * CS_UNUSED; note that this is defined 5.. a valid result ...
	 */
	reslen = 0;

	/*
	 * TODO: add special case for CS_CHAR_TYPE and give different
	 * flags and len
	 */

	/* do convert */
	memset(buffer, 23, sizeof(buffer));
	retcode = cs_convert(ctx, &srcfmt, fromdata, &destfmt, buffer, &reslen);

	/* test result of convert */
	if (tores != retcode) {
		err = "result";
		goto Failed;
	}

	/* test buffer */
	if (todata && (reslen != tolen || memcmp(todata, buffer, tolen) != 0)) {
		int n;
		printf("exp");
		for (n = 0; n < tolen; ++n)
			printf(" %02x", ((unsigned char*)todata)[n]);
		printf("\ngot");
		for (n = 0; n < reslen; ++n)
			printf(" %02x", ((unsigned char*)buffer)[n]);
		printf("\n");

		err = "result data";
		goto Failed;
	}

	/* test result len */
	if (reslen != tolen) {
		int n;
		printf("got");
		for (n = 0; n < reslen; ++n)
			printf(" %02x", ((unsigned char*)buffer)[n]);
		printf("\n");

		err = "result length";
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
	fprintf(stderr, "Test %s failed (got ret=%d len=%d)\n", err, (int) retcode, (int) reslen);
	fprintf(stderr, "line: %d\n  DO_TEST(decl=%s,\n"
		"\t   fromtype=%s,fromdata=%s,fromlen=%s,\n"
		"\t   totype=%s,tomaxlen=%s,\n"
		"\t   tores=%s,todata=%s,tolen=%s);\n",
		line, sdecl, sfromtype, sfromdata, sfromlen, stotype, stomaxlen, stores, stodata, stolen);
	allSuccess = 0;
	return 1;
}

#define DO_TEST(decl,fromtype,fromdata,fromlen,totype,tomaxlen,tores,todata,tolen) { \
 decl; \
 DoTest(fromtype,fromdata,fromlen,totype,tomaxlen,tores,todata,tolen,\
  #decl,#fromtype,#fromdata,#fromlen,#totype,#tomaxlen,#tores,#todata,#tolen,\
  __LINE__);\
}

TEST_MAIN()
{
	volatile CS_BIGINT one = 1;
	int verbose = 1;

	printf("%s: Testing conversion\n", __FILE__);

	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));

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
	DO_TEST(CS_INT test = 12345;
		CS_INT test2 = 12345, 
		CS_INT_TYPE, &test, sizeof(test), CS_INT_TYPE, sizeof(CS_SMALLINT), CS_SUCCEED, &test2, sizeof(test2));

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
#if FREETDS
	DO_TEST(CS_INT test = 32768;
		CS_SMALLINT test2 = 12345, CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_INT test = -32769;
		CS_SMALLINT test2 = 12345, CS_INT_TYPE, &test, sizeof(test), CS_SMALLINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
#endif

	/* biggest and smallest TINYINT */
	DO_TEST(CS_INT test = 255;
		CS_TINYINT test2 = 255,
		CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = 0;
		CS_TINYINT test2 = 0,
		CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	/* overflow */
#if FREETDS
	DO_TEST(CS_INT test = 256;
		CS_TINYINT test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
	DO_TEST(CS_INT test = -1;
		CS_TINYINT test2 = 1, CS_INT_TYPE, &test, sizeof(test), CS_TINYINT_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
#endif

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

#if FREETDS
	DO_TEST(CS_INT test = 1234678; CS_MONEY4 test2 = {
		1234678}
		, CS_INT_TYPE, &test, sizeof(test), CS_MONEY4_TYPE, sizeof(test2), CS_FAIL, NULL, 0);
#endif
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
		test2.mnyhigh = ((one * 1234678) * 10000) >> 32;
		test2.mnylow = (CS_UINT) ((one * 1234678) * 10000),
		CS_INT_TYPE, &test, sizeof(test), CS_MONEY_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));
	DO_TEST(CS_INT test = -8765;
		CS_MONEY test2;
		test2.mnyhigh = ((one * -8765) * 10000) >> 32;
		test2.mnylow = (CS_UINT) ((one * -8765) * 10000),
		CS_INT_TYPE, &test, sizeof(test), CS_MONEY_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	DO_TEST(CS_INT test = 12345;
		CS_CHAR test2[] = "12345",
		CS_INT_TYPE, &test, sizeof(test), CS_CHAR_TYPE, sizeof(test2), CS_SUCCEED, test2, sizeof(test2) - 1);

#if FREETDS
	{ CS_VARCHAR test2 = { 5, "12345"};
	memset(test2.str+5, 23, 251);
	DO_TEST(CS_INT test = 12345,
		CS_INT_TYPE,&test,sizeof(test),
		CS_VARCHAR_TYPE,sizeof(test2),
		CS_SUCCEED,&test2,sizeof(test2));
	}
#endif

	DO_TEST(CS_CHAR test[] = "12345";
		CS_INT test2 = 12345, CS_CHAR_TYPE, test, 5, CS_INT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	/* unterminated number */
	DO_TEST(CS_CHAR test[] = " - 12345";
		CS_INT test2 = -12, CS_CHAR_TYPE, test, 5, CS_INT_TYPE, sizeof(test2), CS_SUCCEED, &test2, sizeof(test2));

	/* to binary */
	DO_TEST(CS_CHAR test[] = "abc";
		CS_CHAR test2[] = "abc", CS_BINARY_TYPE, test, 3, CS_BINARY_TYPE, 3, CS_SUCCEED, test2, 3);
#if 0
	DO_TEST(CS_CHAR test[] = "abcdef";
		CS_CHAR test2[] = "ab", CS_BINARY_TYPE, test, 6, CS_BINARY_TYPE, 2, CS_FAIL, test2, 2);
	DO_TEST(CS_CHAR test[] = "abc";
		CS_CHAR test2[] = "ab", CS_BINARY_TYPE, test, 3, CS_BINARY_TYPE, 2, CS_FAIL, test2, 2);
#endif
	DO_TEST(CS_CHAR test[] = "616263";
		CS_CHAR test2[] = "abc", CS_CHAR_TYPE, test, 6, CS_BINARY_TYPE, 3, CS_SUCCEED, test2, 3);
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_CHAR test2[] = "abc", CS_CHAR_TYPE, test, 12, CS_BINARY_TYPE, 3, CS_FAIL, test2, 3);
	DO_TEST(CS_CHAR test[] = "hello";
		CS_CHAR test2[] = "abc", CS_CHAR_TYPE, test, 12, CS_BINARY_TYPE, 10, CS_FAIL, test2, 0);

	/* to char */
	DO_TEST(CS_INT test = 1234567;
		CS_CHAR test2[] = "1234567", CS_INT_TYPE, &test, sizeof(test), CS_CHAR_TYPE, 7, CS_SUCCEED, test2, 7);
	DO_TEST(CS_CHAR test[] = "abc";
		CS_CHAR test2[] = "ab", CS_CHAR_TYPE, test, 3, CS_CHAR_TYPE, 2, CS_FAIL, test2, 2);
	DO_TEST(CS_CHAR test[] = "abc";
		CS_CHAR test2[] = "616263", CS_BINARY_TYPE, test, 3, CS_CHAR_TYPE, 6, CS_SUCCEED, test2, 6);
	DO_TEST(CS_CHAR test[] = "abcdef";
		CS_CHAR test2[] = "616263", CS_BINARY_TYPE, test, 6, CS_CHAR_TYPE, 6, CS_FAIL, test2, 6);

	/* conversion to various binaries */
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_CHAR test2[12] = "abcdef", CS_CHAR_TYPE, test, 12, CS_IMAGE_TYPE, 12, CS_SUCCEED, test2, 6);
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_CHAR test2[12] = "abcdef", CS_CHAR_TYPE, test, 12, CS_BINARY_TYPE, 12, CS_SUCCEED, test2, 6);
	DO_TEST(CS_CHAR test[] = "616263646566yyy";
		CS_CHAR test2[] = "616263646566xxx", CS_BINARY_TYPE, test, 12, CS_BINARY_TYPE, 12, CS_SUCCEED, test2, 12);
	DO_TEST(CS_CHAR test[] = "6162636465yyyyy";
		CS_CHAR test2[] = "616263646566xxx", CS_BINARY_TYPE, test, 12, CS_BINARY_TYPE, 10, CS_FAIL, test2, 10);
	DO_TEST(CS_CHAR test[] = "6162636465yyyyy";
		CS_CHAR test2[] = "616263646566xxx", CS_IMAGE_TYPE, test, 12, CS_IMAGE_TYPE, 10, CS_FAIL, test2, 10);
	DO_TEST(CS_CHAR test[] = "6162636465yyyyy";
		CS_CHAR test2[] = "616263646566xxx", CS_BINARY_TYPE, test, 12, CS_IMAGE_TYPE, 10, CS_FAIL, test2, 10);
	DO_TEST(CS_CHAR test[] = "6162636465yyyyy";
		CS_CHAR test2[] = "616263646566xxx", CS_IMAGE_TYPE, test, 12, CS_BINARY_TYPE, 10, CS_FAIL, test2, 10);
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_VARBINARY test2; test2.len = 6; memset(test2.array, 23, 256); memcpy(test2.array, "abcdef", 6),
		CS_CHAR_TYPE, test, 12, CS_VARBINARY_TYPE, 12, CS_SUCCEED, &test2, 258);

	/* conversion to various binaries, same as above but with zero padding */
	dest_format = CS_FMT_PADNULL;
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_CHAR test2[12] = "abcdef", CS_CHAR_TYPE, test, 12, CS_IMAGE_TYPE, 12, CS_SUCCEED, test2, 12);
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_CHAR test2[12] = "abcdef", CS_CHAR_TYPE, test, 12, CS_BINARY_TYPE, 12, CS_SUCCEED, test2, 12);
	DO_TEST(CS_CHAR test[] = "616263646566";
		CS_VARBINARY test2; test2.len = 6; memset(test2.array, 23, 256); memcpy(test2.array, "abcdef", 6),
		CS_CHAR_TYPE, test, 12, CS_VARBINARY_TYPE, 12, CS_SUCCEED, &test2, 258);
	dest_format = CS_FMT_UNUSED;

	check_call(ct_exit, (ctx, CS_UNUSED));
	check_call(cs_ctx_drop, (ctx));

	if (verbose && allSuccess) {
		printf("Test succeded\n");
	}
	return allSuccess ? 0 : 1;
}
