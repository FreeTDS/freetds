#include "common.h"

/* test binding with UTF-8 encoding */

#ifndef _WIN32
/* test table name, it contains two japanese characters */
static const char table_name[] = "mytab\xe7\x8e\x8b\xe9\xb4\xbb";

static const char * const strings[] = {
	/* ascii */
	"aaa", "aaa",
	/* latin 1*/
	"abc\xc3\xa9\xc3\xa1\xc3\xb4", "abc\xc3\xa9\xc3\xae\xc3\xb4",
	/* Japanese... */
	"abc\xe7\x8e\x8b\xe9\xb4\xbb", "abc\xe7\x8e\x8b\xe9\xb4\xbb\xe5\x82\x91\xe7\x8e\x8b\xe9\xb4\xbb\xe5\x82\x91",
	NULL, NULL
};

/* same strings in hex */
static const char * const strings_hex[] = {
	/* ascii */
	"0x610061006100", "0x610061006100",
	/* latin 1*/
	"0x610062006300e900e100f400", "0x610062006300e900ee00f400",
	/* Japanese... */
	"0x6100620063008b733b9d", "0x6100620063008b733b9d91508b733b9d9150",
	NULL, NULL
};

static char tmp[1024*3];

static void
TestBinding(int minimun)
{
	const char * const*p;
	SQLINTEGER n;
	SQLLEN n_len;

	sprintf(tmp, "DELETE FROM %s", table_name);
	odbc_command(tmp);

	/* insert with SQLPrepare/SQLBindParameter/SQLExecute */
	sprintf(tmp, "INSERT INTO %s VALUES(?,?,?)", table_name);
	CHKPrepare(T(tmp), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG,
		SQL_INTEGER, 0, 0, &n, 0, &n_len, "S");
	n_len = sizeof(n);
	
	for (n = 1, p = strings; p[0] && p[1]; p += 2, ++n) {
		SQLLEN s1_len, s2_len;
		unsigned int len;

		len = minimun ? ((int) strlen(strings_hex[p-strings]) - 2) / 4 : 40;
		CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_WCHAR, len, 0, (void *) p[0], 0, &s1_len, "S");
		len = minimun ? ((int) strlen(strings_hex[p+1-strings]) - 2) / 4 : 40;
		/* FIXME this with SQL_VARCHAR produce wrong protocol data */
		CHKBindParameter(3, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_WVARCHAR, len, 0, (void *) p[1], 0, &s2_len, "S");
		s1_len = strlen(p[0]);
		s2_len = strlen(p[1]);
		printf("insert #%d\n", (int) n);
		CHKExecute("S");
	}

	/* check rows */
	for (n = 1, p = strings_hex; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "IF NOT EXISTS(SELECT * FROM %s WHERE k = %d AND c = %s AND vc = %s) SELECT 1", table_name, (int) n, p[0], p[1]);
		odbc_check_no_row(tmp);
	}

	odbc_reset_statement();
}

TEST_MAIN()
{
	const char * const*p;
	SQLINTEGER n;

	odbc_use_version3 = true;
	odbc_conn_additional_params = "ClientCharset=UTF-8;";

	odbc_connect();
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		printf("Driver is not FreeTDS, exiting\n");
		odbc_test_skipped();
		return 0;
	}

	if (!odbc_db_is_microsoft() || odbc_db_version_int() < 0x08000000u) {
		odbc_disconnect();
		printf("Test for MSSQL only\n");
		odbc_test_skipped();
		return 0;
	}

	CHKAllocStmt(&odbc_stmt, "S");

	/* create test table */
	sprintf(tmp, "IF OBJECT_ID(N'%s') IS NOT NULL DROP TABLE %s", table_name, table_name);
	odbc_command(tmp);
	sprintf(tmp, "CREATE TABLE %s (k int, c NCHAR(10), vc NVARCHAR(10))", table_name);
	odbc_command(tmp);

	/* insert with INSERT statements */
	for (n = 1, p = strings; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "INSERT INTO %s VALUES (%d,N'%s',N'%s')", table_name, (int) n, p[0], p[1]);
		odbc_command(tmp);
	}

	/* check rows */
	for (n = 1, p = strings_hex; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "IF NOT EXISTS(SELECT * FROM %s WHERE k = %d AND c = %s AND vc = %s) SELECT 1", table_name, (int) n, p[0], p[1]);
		odbc_check_no_row(tmp);
	}

	TestBinding(0);

	TestBinding(1);

	/* cleanup */
	sprintf(tmp, "IF OBJECT_ID(N'%s') IS NOT NULL DROP TABLE %s", table_name, table_name);
	odbc_command(tmp);

	odbc_disconnect();
	printf("Done.\n");
	return 0;
}

#else

TEST_MAIN()
{
	/* on Windows SQLExecDirect is always converted to SQLExecDirectW by the DM */
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif
