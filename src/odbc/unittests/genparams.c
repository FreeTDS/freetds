#include "common.h"
#include <assert.h>
#include <time.h>

/* Test various type from odbc and to odbc */

/*
 * This test is useful to test odbc_sql2tds function using TestInput
 * odbc_sql2tds have some particular cases:
 * (1) char    -> char     handled differently with encoding problems
 * (2) date    -> *        different format TODO
 * (3) numeric -> *        different format
 * (4) *       -> numeric  take precision and scale from ipd TODO
 * (5) *       -> char     test wide
 * (6) *       -> blob     test wchar and ntext
 * (7) *       -> binary   test also with wchar
 * (8) binary  -> *        test also with wchar
 * Also we have to check normal char and wide char
 */

#ifdef TDS_NO_DM
static const char tds_no_dm = 1;
#else
static const char tds_no_dm = 0;
#endif

static char precision = 18;
static char exec_direct = 0;
static char prepare_before = 0;
static char use_cursors = 0;
static int only_test = 0;

static const char *
split_collate(ODBC_BUF** buf, const char **type)
{
	const char *collate = "", *p;

	if ((p = strstr(*type, " COLLATE ")) != NULL) {
		if (odbc_db_is_microsoft())
			collate = p;
		*type = odbc_buf_asprintf(buf, "%.*s", (int) (p - *type), *type);
	}
	return collate;
}

static int
TestOutput(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	const char *sep;
	const char *collate;

	odbc_reset_statement();

	/* build store procedure to test */
	odbc_command("IF OBJECT_ID('spTestProc') IS NOT NULL DROP PROC spTestProc");
	sep = "'";
	if (strncmp(value_to_convert, "0x", 2) == 0 || strncmp(value_to_convert, "NCHAR(", 6) == 0)
		sep = "";
	collate = split_collate(&odbc_buf, &type);
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, %s%s%s)%s",
		type, type, sep, value_to_convert, sep, collate);
	odbc_command(sbuf);
	memset(out_buf, 0, sizeof(out_buf));

	if (use_cursors) {
		odbc_reset_statement();
		CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0, "S");
		CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0, "S");
	}

	/* bind parameter */
	if (exec_direct) {
		CHKBindParameter(1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len, "S");

		/* call store procedure */
		CHKExecDirect(T("{call spTestProc(?)}"), SQL_NTS, "S");
	} else {
		if (prepare_before)
			CHKPrepare(T("{call spTestProc(?)}"), SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len, "S");

		if (!prepare_before)
			CHKPrepare(T("{call spTestProc(?)}"), SQL_NTS, "S");

		CHKExecute("S");
	}

	/*
	 * MS OBDC requires it cause first recordset is a recordset with a
	 * warning caused by the way it execute RPC (via EXEC statement)
	 */
	if (use_cursors && !odbc_driver_is_freetds())
		SQLMoreResults(odbc_stmt);

	/* test results */
	odbc_c2string(sbuf, out_c_type, out_buf, out_len);

	if (strcmp(sbuf, expected) != 0) {
		if (only_test) {
			odbc_command("drop proc spTestProc");
			ODBC_FREE();
			return 1;
		}
		fprintf(stderr, "Wrong result\n  Got: %s\n  Expected: %s\n", sbuf, expected);
		exit(1);
	}
	only_test = 0;
	odbc_command("drop proc spTestProc");
	ODBC_FREE();
	return 0;
}

static char check_truncation = 0;
static char use_nts = 0;

/**
 * Test a parameter as input to prepared statement
 *
 * "value_to_convert" will be inserted as database type "type" and C type "out_c_type" into a column of
 * database type "param_type" and SQL type "out_sql_type". Then a select statement check that the column
 * in the database is the same as "value_to_convert".
 * "value_to_convert" can also contain a syntax like "input -> output", in this case the initial inserted
 * value is "input" and the final check will check for "output".
 */
static void
TestInput(SQLSMALLINT out_c_type, const char *type, SQLSMALLINT out_sql_type, const char *param_type, const char *value_to_convert)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	const char *expected = value_to_convert;
	size_t value_len = strlen(value_to_convert);
	const char *p;
	const char *sep = "'";
	const char *collate;

	odbc_reset_statement();

	/* execute a select to get data as wire */
	if ((p = strstr(value_to_convert, " -> ")) != NULL) {
		value_len = p - value_to_convert;
		expected = p + 4;
	}
	if (value_len >= 2 && strncmp(value_to_convert, "0x", 2) == 0)
		sep = "";
	collate = split_collate(&odbc_buf, &type);
	sprintf(sbuf, "SELECT CONVERT(%s, %s%.*s%s%s)", type, sep, (int) value_len, value_to_convert, sep, collate);
	odbc_command(sbuf);
	SQLBindCol(odbc_stmt, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	CHKFetch("SI");
	CHKFetch("No");
	CHKMoreResults("No");
	if (use_nts) {
		out_len = SQL_NTS;
		use_nts = 0;
	}

	/* create a table with a column of that type */
	odbc_reset_statement();
	collate = split_collate(&odbc_buf, &param_type);
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s%s)", param_type, collate);
	odbc_command(sbuf);

	if (use_cursors) {
		odbc_reset_statement();
		CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0, "S");
		CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0, "S");
	}

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (exec_direct) {
		CHKBindParameter(1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len, "S");

		if (check_truncation)
			CHKExecDirect(T(sbuf), SQL_NTS, "E");
		else
			CHKExecDirect(T(sbuf), SQL_NTS, "SNo");
	} else {
		if (prepare_before)
			CHKPrepare(T(sbuf), SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len, "S");

		if (!prepare_before)
			CHKPrepare(T(sbuf), SQL_NTS, "S");

		if (check_truncation)
			CHKExecute("E");
		else
			CHKExecute("SNo");
	}

	/* check if row is present */
	if (!check_truncation) {
		char *p;

		odbc_reset_statement();
		sep = "'";
		if (strncmp(expected, "0x", 2) == 0)
			sep = "";

		strcpy(sbuf, "SELECT * FROM #tmp_insert WHERE ");
		p = strchr(sbuf, 0);
		if (strcmp(param_type, "TEXT") == 0)
			sprintf(p, "CONVERT(VARCHAR(255), col) = CONVERT(VARCHAR(255), %s%s%s)", sep, expected, sep);
		else if (strcmp(param_type, "NTEXT") == 0)
			sprintf(p, "CONVERT(NVARCHAR(2000), col) = CONVERT(NVARCHAR(2000), %s%s%s)", sep, expected, sep);
		else if (strcmp(param_type, "IMAGE") == 0)
			sprintf(p, "CONVERT(VARBINARY(255), col) = CONVERT(VARBINARY(255), %s%s%s)", sep, expected, sep);
		else
			sprintf(p, "col = CONVERT(%s, %s%s%s)", param_type, sep, expected, sep);
		odbc_command(sbuf);

		CHKFetch("S");
		CHKFetch("No");
		CHKMoreResults("No");
	}
	check_truncation = 0;
	odbc_command("DROP TABLE #tmp_insert");
	ODBC_FREE();
}

/* stripped down version of TestInput for NULLs */
static void
NullInput(SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *param_type)
{
	char sbuf[1024];
	SQLLEN out_len = SQL_NULL_DATA;

	odbc_reset_statement();

	/* create a table with a column of that type */
	odbc_reset_statement();
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s NULL)", param_type);
	odbc_command(sbuf);

	if (use_cursors) {
		odbc_reset_statement();
		CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0, "S");
		CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0, "S");
	}

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (exec_direct) {
		CHKBindParameter(1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, NULL, 1, &out_len, "S");

		CHKExecDirect(T(sbuf), SQL_NTS, "SNo");
	} else {
		if (prepare_before)
			CHKPrepare(T(sbuf), SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, NULL, 1, &out_len, "S");

		if (!prepare_before)
			CHKPrepare(T(sbuf), SQL_NTS, "S");

		CHKExecute("SNo");
	}

	/* check if row is present */
	odbc_reset_statement();
	if (!odbc_db_is_microsoft() && strcmp(param_type, "TEXT") == 0)
		odbc_command("SELECT * FROM #tmp_insert WHERE DATALENGTH(col) = 0 OR DATALENGTH(col) IS NULL");
	else
		odbc_command("SELECT * FROM #tmp_insert WHERE col IS NULL");

	CHKFetch("S");
	CHKFetch("No");
	CHKMoreResults("No");
	odbc_command("DROP TABLE #tmp_insert");
	ODBC_FREE();
}


static int big_endian = 1;

static const char*
pack(const char *fmt, ...)
{
	static char out[80];
	char *p = out;

	va_list v;
	va_start(v, fmt);
	for (; *fmt; ++fmt) {
		unsigned n = va_arg(v, unsigned);
		int i, l = 2;

		assert(p - out + 8 < sizeof(out));
		switch (*fmt) {
		case 'l':
			l += 2;
		case 's':
			for (i = 0; i < l; ++i) {
				sprintf(p, "%02X", (n >> (8*(big_endian ? l-1-i : i))) & 0xffu);
				p += 2;
			}
			break;
		default:
			assert(0);
		}
	}
	*p = 0;
	va_end(v);
	return out;
}

static void
AllTests(void)
{
	struct tm *ltime;
	char buf[80];
	time_t curr_time;

	SQLINTEGER y, m, d;
	char date[128];

	printf("use_cursors %d exec_direct %d prepare_before %d\n", use_cursors, exec_direct, prepare_before);

	/* test some NULLs */
	NullInput(SQL_C_CHAR, SQL_VARCHAR, "VARCHAR(100)");
	NullInput(SQL_C_CHAR, SQL_LONGVARCHAR, "TEXT");
	NullInput(SQL_C_LONG, SQL_INTEGER, "INTEGER");
	NullInput(SQL_C_LONG, SQL_LONGVARCHAR, "TEXT");
	NullInput(SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, "DATETIME");
	NullInput(SQL_C_FLOAT,  SQL_REAL, "FLOAT");
	NullInput(SQL_C_NUMERIC, SQL_LONGVARCHAR, "TEXT");
	if (odbc_db_is_microsoft() && odbc_db_version_int() >= 0x08000000u)
		NullInput(SQL_C_BIT, SQL_BIT, "BIT");
	NullInput(SQL_C_DOUBLE, SQL_DOUBLE, "MONEY");

	/* FIXME why should return 38 0 as precision and scale ?? correct ?? */
	precision = 18;
	TestOutput("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");
	TestOutput("DECIMAL(18,2)", "123", SQL_C_NUMERIC, SQL_DECIMAL, "18 0 1 7B");
	precision = 38;
	TestOutput("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "38 0 1 7B");
	TestInput(SQL_C_LONG, "INTEGER", SQL_VARCHAR, "VARCHAR(20)", "12345");
	TestInput(SQL_C_LONG, "INTEGER", SQL_LONGVARCHAR, "TEXT", "12345");
	/*
	 * MS driver behavior for output parameters is different
	 * former returns "313233" while newer "333133323333"
	 */
	if (odbc_driver_is_freetds())
		TestOutput("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "333133323333");

	only_test = 1;
	precision = 3;
	if (TestOutput("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, pack("ssssssl", 2004, 2, 24, 15, 16, 17, 0))) {
		/* FIXME our driver ignore precision for date */
		precision = 3;
		/* Some MS driver incorrectly prepare with smalldatetime*/
		if (!use_cursors || odbc_driver_is_freetds()) {
				TestOutput("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, pack("ll", 0x9497, 0xFBAA2C));
		}
		TestOutput("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, pack("ll", 0x9497, 0xFB9640));
	} else {
		TestOutput("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, pack("ssssssl", 2004, 2, 24, 15, 16, 0, 0));
	}
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34");

	/* test timestamp millisecond round off */
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.001 -> 2005-07-22 09:51:34.000");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.002 -> 2005-07-22 09:51:34.003");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.003 -> 2005-07-22 09:51:34.003");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.004 -> 2005-07-22 09:51:34.003");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.005 -> 2005-07-22 09:51:34.007");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34.006 -> 2005-07-22 09:51:34.007");

	/* FIXME on ms driver first SQLFetch return SUCCESS_WITH_INFO for truncation error */
	TestInput(SQL_C_TYPE_DATE, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 13:02:03 -> 2005-07-22 00:00:00");

	/* replace date information with current date */
	time(&curr_time);
	ltime = localtime(&curr_time);
	y = ltime->tm_year + 1900;
	m = ltime->tm_mon + 1;
	d = ltime->tm_mday;
	/* server concept of data can be different so try ask to server */
	odbc_command("SELECT GETDATE()");
	SQLBindCol(odbc_stmt, 1, SQL_C_CHAR, date, sizeof(date), NULL);
	if (SQLFetch(odbc_stmt) == SQL_SUCCESS) {
		int a, b, c;
		if (sscanf(date, "%d-%d-%d", &a, &b, &c) == 3) {
			y = a;
			m = b;
			d = c;
		}
	}
	SQLFetch(odbc_stmt);
	SQLMoreResults(odbc_stmt);
	SQLFreeStmt(odbc_stmt, SQL_UNBIND);
	sprintf(buf, "2003-07-22 13:02:03 -> %04d-%02d-%02d 13:02:03", (int) y, (int) m, (int) d);
	TestInput(SQL_C_TYPE_TIME, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", buf);

	TestInput(SQL_C_FLOAT,  "FLOAT", SQL_REAL, "FLOAT", "1234.25");
	TestInput(SQL_C_DOUBLE, "REAL", SQL_REAL, "FLOAT", "-1234.25");
	TestInput(SQL_C_FLOAT,  "REAL", SQL_REAL, "FLOAT", "1234.25");
	TestInput(SQL_C_DOUBLE, "FLOAT", SQL_REAL, "FLOAT", "-1234.25");
	TestInput(SQL_C_FLOAT,  "FLOAT", SQL_FLOAT, "FLOAT", "1234.25");
	TestInput(SQL_C_DOUBLE, "REAL", SQL_FLOAT, "FLOAT", "-1234.25");
	TestInput(SQL_C_FLOAT,  "FLOAT", SQL_DOUBLE, "FLOAT", "1234.25");
	TestInput(SQL_C_DOUBLE, "REAL", SQL_DOUBLE, "FLOAT", "-1234.25");

	TestInput(SQL_C_UTINYINT, "TINYINT", SQL_TINYINT, "TINYINT", "231");

	TestInput(SQL_C_NUMERIC, "NUMERIC(20,3)", SQL_NUMERIC, "NUMERIC(20,3)", "765432.2 -> 765432");
	TestInput(SQL_C_NUMERIC, "NUMERIC(20,3)", SQL_VARCHAR, "VARCHAR(20)", "578246.234 -> 578246");
	TestInput(SQL_C_NUMERIC, "NUMERIC(20,3)", SQL_LONGVARCHAR, "TEXT", "578246.234 -> 578246");

	TestInput(SQL_C_CHAR, "VARCHAR(100)", SQL_VARBINARY, "VARBINARY(20)", "4145544F -> AETO");
	TestInput(SQL_C_CHAR, "TEXT COLLATE Latin1_General_CI_AS", SQL_VARBINARY, "VARBINARY(20)", "4145544F -> AETO");
	TestInput(SQL_C_CHAR, "VARCHAR(100)", SQL_LONGVARBINARY, "IMAGE", "4145544F -> AETO");
	TestInput(SQL_C_BINARY, "VARBINARY(100)", SQL_VARCHAR, "VARCHAR(20)", "0x4145544F -> AETO");
	TestInput(SQL_C_BINARY, "IMAGE", SQL_VARCHAR, "VARCHAR(20)", "0x4145544F -> AETO");

	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "0");
	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "1");

	TestInput(SQL_C_DOUBLE, "MONEY", SQL_DOUBLE, "MONEY", "123.34");

	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_VARCHAR, "VARCHAR(20)", "1EasyTest");
	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_LONGVARCHAR, "TEXT", "1EasyTest");
	TestInput(SQL_C_WCHAR, "VARCHAR(10)", SQL_VARCHAR, "VARCHAR(10)", "Test 12345");
	TestInput(SQL_C_WCHAR, "VARCHAR(10)", SQL_LONGVARCHAR, "TEXT", "Test 12345");
	/* TODO use collate in syntax if available */
	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_VARCHAR, "VARCHAR(20)", "me\xf4");
	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_LONGVARCHAR, "TEXT", "me\xf4");

	precision = 6;
	/* output from char with conversions */
	TestOutput("VARCHAR(20)", "foo test", SQL_C_CHAR, SQL_VARCHAR, "6 foo te");
	/* TODO use collate in syntax if available */
	/* while usually on Microsoft database this encoding is valid on Sybase the database
	 * could use UTF-8 encoding where \xf8\xf9 is an invalid encoded string */
	if (odbc_db_is_microsoft() && odbc_tds_version() > 0x700)
		TestOutput("VARCHAR(20) COLLATE Latin1_General_CI_AS", "NCHAR(0xf8) + NCHAR(0xf9)", SQL_C_CHAR, SQL_VARCHAR, "2 \xf8\xf9");

	/* MSSQL 2000 using ptotocol 7.1+ */
	if ((odbc_db_is_microsoft() && odbc_db_version_int() >= 0x08000000u && odbc_tds_version() > 0x700)
	    || (!odbc_db_is_microsoft() && strncmp(odbc_db_version(), "15.00.", 6) >= 0)) {
		TestOutput("BIGINT", "-987654321065432", SQL_C_BINARY, SQL_BIGINT, big_endian ? "FFFC7DBBCF083228" : "283208CFBB7DFCFF");
		TestInput(SQL_C_SBIGINT, "BIGINT", SQL_BIGINT, "BIGINT", "-12345678901234");
	}
	/* MSSQL 2000 */
	if (odbc_db_is_microsoft() && odbc_db_version_int() >= 0x08000000u) {
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "test");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WLONGVARCHAR, "NTEXT", "test");
		/* test for invalid stream due to truncation*/
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "01234567890");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WLONGVARCHAR, "NTEXT", "01234567890");
#ifdef ENABLE_DEVELOPING
		check_truncation = 1;
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "012345678901234567890");
		check_truncation = 1;
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WLONGVARCHAR, "NTEXT", "012345678901234567890");
#endif
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "\xa3h\xf9 -> 0xA3006800f900");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WLONGVARCHAR, "NTEXT", "\xa3h\xf9 -> 0xA3006800f900");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "0xA3006800f900 -> \xa3h\xf9");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WLONGVARCHAR, "NTEXT", "0xA3006800f900 -> \xa3h\xf9");

		TestInput(SQL_C_LONG, "INT", SQL_WVARCHAR, "NVARCHAR(100)", "45236");
		TestInput(SQL_C_LONG, "INT", SQL_WLONGVARCHAR, "NTEXT", "45236");

		precision = 6;
		TestOutput("NVARCHAR(20)", "foo test", SQL_C_CHAR, SQL_WVARCHAR, "6 foo te");
		precision = 12;
		TestOutput("NVARCHAR(20)", "foo test", SQL_C_CHAR, SQL_WVARCHAR, "8 foo test");
		/* TODO use collate in syntax if available */
		TestOutput("NVARCHAR(20)", "0xf800f900", SQL_C_CHAR, SQL_WVARCHAR, "2 \xf8\xf9");

		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WVARCHAR, "NVARCHAR(10)", "1EasyTest2");
		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WLONGVARCHAR, "NTEXT", "1EasyTest2");
		use_nts = 1;
		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WVARCHAR, "NVARCHAR(10)", "1EasyTest3");
		use_nts = 1;
		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WLONGVARCHAR, "NTEXT", "1EasyTest3");
		TestInput(SQL_C_WCHAR, "NVARCHAR(3)", SQL_WVARCHAR, "NVARCHAR(3)", "0xf800a300bc06");
		TestInput(SQL_C_WCHAR, "NVARCHAR(3)", SQL_WLONGVARCHAR, "NTEXT", "0xf800a300bc06");

		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_INTEGER, "INT", " -423785  -> -423785");

		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_VARBINARY, "VARBINARY(20)", "4145544F -> AETO");
		TestInput(SQL_C_CHAR, "NTEXT COLLATE Latin1_General_CI_AS", SQL_VARBINARY, "VARBINARY(20)", "4145544F -> AETO");

		TestInput(SQL_C_BINARY, "VARBINARY(100)", SQL_WVARCHAR, "NVARCHAR(20)", "0x4100450054004F00 -> AETO");
		TestInput(SQL_C_BINARY, "IMAGE", SQL_WVARCHAR, "NVARCHAR(20)", "0x4100450054004F00 -> AETO");
	}
	/* MSSQL 2005 */
	if (odbc_db_is_microsoft() && odbc_db_version_int() >= 0x09000000u) {
		TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_LONGVARCHAR, "VARCHAR(MAX)", "1EasyTest");
		TestInput(SQL_C_BINARY, "VARBINARY(20)", SQL_LONGVARBINARY, "VARBINARY(MAX)", "Anything will suite!");
		TestInput(SQL_C_CHAR, "MONEY", SQL_VARCHAR, "MONEY", "123.3456");
	}
	/* MSSQL 2008 */
	if (odbc_db_is_microsoft() && odbc_db_version_int() >= 0x0a000000u) {
		TestInput(SQL_C_TYPE_DATE, "DATE", SQL_TYPE_DATE, "DATE", "2005-07-22");
		TestInput(SQL_C_TYPE_TIME, "TIME", SQL_TYPE_TIME, "TIME", "13:02:03");
	}

	/* Sybase */
	if (!odbc_db_is_microsoft()) {
		TestInput(SQL_C_CHAR, "UNIVARCHAR(100)", SQL_WCHAR, "UNIVARCHAR(100)", "test");
		TestInput(SQL_C_WCHAR, "UNIVARCHAR(100)", SQL_WCHAR, "UNIVARCHAR(100)", "test");
		TestInput(SQL_C_CHAR, "UNIVARCHAR(100)", SQL_WVARCHAR, "UNIVARCHAR(100)", "test");
		TestInput(SQL_C_WCHAR, "UNIVARCHAR(100)", SQL_WVARCHAR, "UNIVARCHAR(100)", "test");
	}
}

int
main(void)
{
	odbc_use_version3 = 1;
	odbc_conn_additional_params = "ClientCharset=ISO-8859-1;";

	odbc_connect();

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;

	for (use_cursors = 0; use_cursors <= 1; ++use_cursors) {
		if (use_cursors) {
			if (!tds_no_dm || !odbc_driver_is_freetds())
				odbc_reset_statement();
			/* if connection does not support cursors returns success */
			setenv("TDS_SKIP_SUCCESS", "1", 1);
			odbc_check_cursor();
		}

		exec_direct = 1;
		AllTests();

		exec_direct = 0;
		prepare_before = 1;
		AllTests();

		prepare_before = 0;
		AllTests();
	}

	odbc_disconnect();

	printf("Done successfully!\n");
	return 0;
}

