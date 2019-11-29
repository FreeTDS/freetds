#include "common.h"
#include <assert.h>
#include <freetds/utils/string.h>
#include <freetds/odbc.h>

/* test some internal funcions */

#ifdef _WIN32
HINSTANCE hinstFreeTDS;
#endif

int
main(int argc, char *argv[])
{
#ifdef ENABLE_ODBC_WIDE
	DSTR s;
	SQLWCHAR outbuf[16];
	SQLINTEGER outlen;
	tds_dstr_init(&s);

#ifdef _WIN32
	hinstFreeTDS = GetModuleHandle(NULL);
#endif

	/* just allocate handles, we don't need to connect */
	CHKAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocHandle(SQL_HANDLE_DBC, odbc_env, &odbc_conn, "S");

	/* check is FreeTDS, if not just return */
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		return 0;
	}

	odbc_dstr_copy_flag(odbc_conn, &s, 3, (ODBC_CHAR*) "foo", 0);
#define WIDE_TEST(chars, exp) do { \
	SQLWCHAR input[] = chars; \
	odbc_dstr_copy_flag(odbc_conn, &s, TDS_VECTOR_SIZE(input), (ODBC_CHAR*) input, 1); \
	if (strlen(exp) != tds_dstr_len(&s) || strcmp(exp, tds_dstr_cstr(&s)) != 0) { \
		fprintf(stderr, "%d: Wrong, len %u: %s\n", __LINE__, (unsigned) tds_dstr_len(&s), tds_dstr_cstr(&s)); \
		return 1; \
	} \
	outlen = -1; \
	odbc_set_string_flag(odbc_conn, outbuf, TDS_VECTOR_SIZE(outbuf), &outlen, tds_dstr_cstr(&s), tds_dstr_len(&s), 0x11); \
	if (outlen < 0 || outlen != TDS_VECTOR_SIZE(input) || memcmp(outbuf, input, sizeof(input)) != 0) { \
		fprintf(stderr, "%d: out_len %u %x %x %x\n", __LINE__, outlen, outbuf[0], outbuf[1], outbuf[2]); \
		return 1; \
	} \
} while(0)
	odbc_dstr_copy_flag(odbc_conn, &s, 3, (ODBC_CHAR*) "f\0o\0o\0", 1);
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
#define SEP ,
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

