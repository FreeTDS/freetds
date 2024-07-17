#include "common.h"
#include <assert.h>
#include <freetds/utils/string.h>
#include <freetds/odbc.h>

/* test some internal funcions */

#ifdef ENABLE_ODBC_WIDE
static void
wide_test(const SQLWCHAR* input, size_t input_len, const char *exp, int line)
{
	DSTR s = DSTR_INITIALIZER;
	SQLWCHAR outbuf[16];
	SQLINTEGER outlen;

	odbc_dstr_copy_flag((TDS_DBC *) odbc_conn, &s, (int) input_len, (ODBC_CHAR*) input, 1);
	if (strlen(exp) != tds_dstr_len(&s) || strcmp(exp, tds_dstr_cstr(&s)) != 0) {
		fprintf(stderr, "%d: Wrong, len %u: %s\n", line,
			(unsigned) tds_dstr_len(&s), tds_dstr_cstr(&s));
		exit(1);
	}
	outlen = -1;
	odbc_set_string_flag((TDS_DBC *) odbc_conn, outbuf, TDS_VECTOR_SIZE(outbuf), &outlen,
			     tds_dstr_cstr(&s), (int) tds_dstr_len(&s), 0x11);
	if (outlen < 0 || outlen !=input_len
	    || memcmp(outbuf, input, input_len * sizeof(input[0])) != 0) {
		fprintf(stderr, "%d: out_len %u %x %x %x\n", line, (unsigned) outlen, outbuf[0], outbuf[1], outbuf[2]);
		exit(1);
	}
	tds_dstr_free(&s);
}
#endif

TEST_MAIN()
{
#ifdef ENABLE_ODBC_WIDE
	DSTR s = DSTR_INITIALIZER;

	/* just allocate handles, we don't need to connect */
	CHKAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocHandle(SQL_HANDLE_DBC, odbc_env, &odbc_conn, "S");

	/* check is FreeTDS, if not just return */
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		return 0;
	}

	odbc_dstr_copy_flag((TDS_DBC *) odbc_conn, &s, 3, (ODBC_CHAR*) "foo", 0);
	assert(strcmp("foo", tds_dstr_cstr(&s)) == 0);

#define WIDE_TEST(chars, exp) do { \
	static const SQLWCHAR input[] = chars; \
	wide_test(input, TDS_VECTOR_SIZE(input), exp, __LINE__); \
} while(0)
#define SEP ,

	WIDE_TEST({ 'f' SEP 'o' SEP 'o' }, "foo");
	WIDE_TEST({ 0x41 }, "A");
	WIDE_TEST({ 0xA1 }, "\xc2\xA1");
	WIDE_TEST({ 0x81 }, "\xc2\x81");
	WIDE_TEST({ 0x101 }, "\xc4\x81");
	WIDE_TEST({ 0x201 }, "\xc8\x81");
	WIDE_TEST({ 0x401 }, "\xd0\x81");
	WIDE_TEST({ 0x801 }, "\xe0\xa0\x81");
	WIDE_TEST({ 0x1001 }, "\xe1\x80\x81");
	WIDE_TEST({ 0x2001 }, "\xe2\x80\x81");
	WIDE_TEST({ 0x4001 }, "\xe4\x80\x81");
	WIDE_TEST({ 0x8001 }, "\xe8\x80\x81");
#if SIZEOF_SQLWCHAR == 2
	WIDE_TEST({ 0xd800 SEP 0xdc01 }, "\xf0\x90\x80\x81");
	WIDE_TEST({ 0xd800 SEP 0xdd01 }, "\xf0\x90\x84\x81");
	WIDE_TEST({ 0xd840 SEP 0xdd01 }, "\xf0\xa0\x84\x81");
	WIDE_TEST({ 0xd8c0 SEP 0xdd01 }, "\xf1\x80\x84\x81");
	WIDE_TEST({ 0xd9c0 SEP 0xdd01 }, "\xf2\x80\x84\x81");
#else
	WIDE_TEST({ 0x10001 }, "\xf0\x90\x80\x81");
#endif

	tds_dstr_free(&s);
	odbc_disconnect();
#endif
	return 0;
}

