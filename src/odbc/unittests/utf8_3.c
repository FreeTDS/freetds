#include "common.h"
#include <assert.h>

/* test conversion using SQLGetData */

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocConnect(&odbc_conn, "S");
}

int
main(int argc, char *argv[])
{
	char tmp[512*4+64];
	SQLSMALLINT slen;
	SQLLEN len;
	unsigned char buf[32];
	static const char expected[] = "\xf0\x9f\x8e\x84";
	int i;

	if (odbc_read_login_info())
		exit(1);

	/* connect string using DSN */
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;ClientCharset=UTF-8;", odbc_server, odbc_user, odbc_password, odbc_database);
	CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &slen, SQL_DRIVER_NOPROMPT, "SI");
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		printf("Driver is not FreeTDS, exiting\n");
		odbc_test_skipped();
		return 0;
	}

	if (!odbc_db_is_microsoft() || odbc_tds_version() < 0x700) {
		odbc_disconnect();
		/* we need NVARCHAR */
		printf("Test for MSSQL only using protocol 7.0\n");
		odbc_test_skipped();
		return 0;
	}

	CHKAllocStmt(&odbc_stmt, "S");

	/* a Christmas tree */
	odbc_command("SELECT CONVERT(NVARCHAR(10), CONVERT(VARBINARY(20), 0x3CD884DF))");

	CHKFetch("S");

	/* read one byte at a time and test it */
	for (i = 0; i < 4; ++i) {
		memset(buf, '-', sizeof(buf));
		CHKGetData(1, SQL_C_CHAR, buf, 2, &len, i < 3 ? "I" : "S");
		printf("res %ld buf { 0x%02x, 0x%02x }\n", (long int) len, buf[0], buf[1]);
		assert(len == SQL_NO_TOTAL || len == 4 - i);
		assert(buf[0] == (unsigned char) expected[i]);
		assert(buf[1] == 0);
	}
	CHKGetData(1, SQL_C_CHAR, buf, 2, &len, "No");

	odbc_disconnect();
	return 0;
}

