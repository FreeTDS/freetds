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
 * (6) *       -> blob     test wchar and ntext TODO
 * (7) *       -> binary   test also with wchar TODO
 * (8) binary  -> *        test also with wchar TODO
 * Also we have to check normal char and wide char
 */

static char software_version[] = "$Id: genparams.c,v 1.35 2008-10-27 14:27:01 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#ifdef TDS_NO_DM
static const char tds_no_dm = 1;
#else
static const char tds_no_dm = 0;
#endif

static char precision = 18;
static char exec_direct = 0;
static char prepare_before = 0;
static char use_cursors = 0;

static void
TestOutput(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;
	const char *sep;

	ResetStatement();

	/* build store procedure to test */
	Command(Statement, "IF OBJECT_ID('spTestProc') IS NOT NULL DROP PROC spTestProc");
	sep = "'";
	if (strncmp(value_to_convert, "0x", 2) == 0)
		sep = "";
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, %s%s%s)", type, type, sep, value_to_convert, sep);
	Command(Statement, sbuf);
	memset(out_buf, 0, sizeof(out_buf));

	if (use_cursors) {
		ResetStatement();
		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0));
		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0));
	}

	/* bind parameter */
	if (exec_direct) {
		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len));

		/* call store procedure */
		CHK(SQLExecDirect, (Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS));
	} else {
		if (prepare_before)
			CHK(SQLPrepare, (Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS));

		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len));

		if (!prepare_before)
			CHK(SQLPrepare, (Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS));

		CHK(SQLExecute, (Statement));
	}

	/* test results */
	sbuf[0] = 0;
	switch (out_c_type) {
	case SQL_C_NUMERIC:
		num = (SQL_NUMERIC_STRUCT *) out_buf;
		sprintf(sbuf, "%d %d %d ", num->precision, num->scale, num->sign);
		i = SQL_MAX_NUMERIC_LEN;
		for (; i > 0 && !num->val[--i];);
		for (; i >= 0; --i)
			sprintf(strchr(sbuf, 0), "%02X", num->val[i]);
		break;
	case SQL_C_BINARY:
		assert(out_len >= 0);
		for (i = 0; i < out_len; ++i)
			sprintf(strchr(sbuf, 0), "%02X", (int) out_buf[i]);
		break;
	case SQL_C_CHAR:
		sprintf(sbuf, "%d %s", (int) out_len, out_buf);
		break;
	default:
		/* not supported */
		assert(0);
		break;
	}

	if (strcmp(sbuf, expected) != 0) {
		fprintf(stderr, "Wrong result\n  Got: %s\n  Expected: %s\n", sbuf, expected);
		exit(1);
	}
	Command(Statement, "drop proc spTestProc");
}

static char check_truncation = 0;
static char use_nts = 0;

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

	ResetStatement();

	/* execute a select to get data as wire */
	if ((p = strstr(value_to_convert, " -> ")) != NULL) {
		value_len = p - value_to_convert;
		expected = p + 4;
	}
	if (value_len >= 2 && strncmp(value_to_convert, "0x", 2) == 0)
		sep = "";
	sprintf(sbuf, "SELECT CONVERT(%s, %s%.*s%s)", type, sep, (int) value_len, value_to_convert, sep);
	Command(Statement, sbuf);
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		ODBC_REPORT_ERROR("Expected row");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Row not expected");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Recordset not expected");
	if (use_nts) {
		out_len = SQL_NTS;
		use_nts = 0;
	}

	/* create a table with a column of that type */
	ResetStatement();
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s)", param_type);
	Command(Statement, sbuf);

	if (use_cursors) {
		ResetStatement();
		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0));
		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0));
	}

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (exec_direct) {
		SQLRETURN rc;

		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len));

		rc = SQLExecDirect(Statement, (SQLCHAR *) sbuf, SQL_NTS);
		if (check_truncation) {
			if (rc != SQL_ERROR)
				ODBC_REPORT_ERROR("SQLExecDirect should return error!");
		} else if (rc != SQL_SUCCESS && rc != SQL_NO_DATA)
			ODBC_REPORT_ERROR("SQLExecDirect() failure!");
	} else {
		SQLRETURN rc;

		if (prepare_before)
			CHK(SQLPrepare, (Statement, (SQLCHAR *) sbuf, SQL_NTS));

		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len));

		if (!prepare_before)
			CHK(SQLPrepare, (Statement, (SQLCHAR *) sbuf, SQL_NTS));

		rc = SQLExecute(Statement);
		if (check_truncation) {
			if (rc != SQL_ERROR)
				ODBC_REPORT_ERROR("SQLExecute should return error!");
		} else if (rc != SQL_SUCCESS && rc != SQL_NO_DATA)
			ODBC_REPORT_ERROR("SQLExecute() failure!");
	}

	/* check is row is present */
	if (!check_truncation) {
		ResetStatement();
		sep = "'";
		if (strncmp(expected, "0x", 2) == 0)
			sep = "";
		sprintf(sbuf, "SELECT * FROM #tmp_insert WHERE col = CONVERT(%s, %s%s%s)", param_type, sep, expected, sep);
		Command(Statement, sbuf);

		CHK(SQLFetch, (Statement));
		if (SQLFetch(Statement) != SQL_NO_DATA)
			ODBC_REPORT_ERROR("Row not expected");
		if (SQLMoreResults(Statement) != SQL_NO_DATA)
			ODBC_REPORT_ERROR("Recordset not expected");
	}
	check_truncation = 0;
	Command(Statement, "DROP TABLE #tmp_insert");
}

static int big_endian = 1;
static char version[32];

static void
AllTests(void)
{
	struct tm *ltime;
	char buf[80];
	time_t curr_time;

	SQLINTEGER y, m, d;
	char date[128];

	printf("use_cursors %d exec_direct %d prepare_before %d\n", use_cursors, exec_direct, prepare_before);

	/* FIXME why should return 38 0 as precision and scale ?? correct ?? */
	precision = 18;
	TestOutput("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");
	TestOutput("DECIMAL(18,2)", "123", SQL_C_NUMERIC, SQL_DECIMAL, "18 0 1 7B");
	precision = 38;
	TestOutput("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "38 0 1 7B");
	TestInput(SQL_C_LONG, "INTEGER", SQL_VARCHAR, "VARCHAR(20)", "12345");
	/* MS driver behavior for output parameters is different */
	if (driver_is_freetds())
		TestOutput("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "333133323333");
	else
		TestOutput("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "313233");
	/* FIXME our driver ignore precision for date */
	precision = 3;
	TestOutput("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, big_endian ? "0000949700FBAA2C" : "979400002CAAFB00");
	TestOutput("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP,
	     big_endian ? "0000949700FB9640" : "979400004096FB00");
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
	Command(Statement, "SELECT GETDATE()");
	SQLBindCol(Statement, 1, SQL_C_CHAR, date, sizeof(date), NULL);
	if (SQLFetch(Statement) == SQL_SUCCESS) {
		int a, b, c;
		if (sscanf(date, "%d-%d-%d", &a, &b, &c) == 3) {
			y = a;
			m = b;
			d = c;
		}
	}
	SQLFetch(Statement);
	SQLMoreResults(Statement);
	SQLFreeStmt(Statement, SQL_UNBIND);
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

	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "0");
	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "1");

	TestInput(SQL_C_DOUBLE, "MONEY", SQL_DOUBLE, "MONEY", "123.34");

	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_VARCHAR, "VARCHAR(20)", "1EasyTest");
	TestInput(SQL_C_WCHAR, "VARCHAR(10)", SQL_VARCHAR, "VARCHAR(10)", "Test 12345");
	/* TODO use collate in syntax if available */
	TestInput(SQL_C_CHAR, "VARCHAR(20)", SQL_VARCHAR, "VARCHAR(20)", "me\xf4");

	precision = 6;
	/* output from char with conversions */
	TestOutput("VARCHAR(20)", "foo test", SQL_C_CHAR, SQL_VARCHAR, "6 foo te");
	/* TODO use collate in sintax if available */
	TestOutput("VARCHAR(20)", "0xf8f9", SQL_C_CHAR, SQL_VARCHAR, "2 \xf8\xf9");

	/* TODO some Sybase versions */
	if (db_is_microsoft() && (strncmp(version, "08.00.", 6) == 0 || strncmp(version, "09.00.", 6) == 0)) {
		TestOutput("BIGINT", "-987654321065432", SQL_C_BINARY, SQL_BIGINT, big_endian ? "FFFC7DBBCF083228" : "283208CFBB7DFCFF");
		TestInput(SQL_C_SBIGINT, "BIGINT", SQL_BIGINT, "BIGINT", "-12345678901234");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "test");
		/* test for invalid stream due to truncation*/
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "01234567890");
#ifdef ENABLE_DEVELOPING
		check_truncation = 1;
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "012345678901234567890");
#endif
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "\xa3h\xf9 -> 0xA3006800f900");
		TestInput(SQL_C_CHAR, "NVARCHAR(100)", SQL_WCHAR, "NVARCHAR(100)", "0xA3006800f900 -> \xa3h\xf9");

		TestInput(SQL_C_LONG, "INT", SQL_WVARCHAR, "NVARCHAR(100)", "45236");

		precision = 6;
		TestOutput("NVARCHAR(20)", "foo test", SQL_C_CHAR, SQL_WVARCHAR, "6 foo te");
		precision = 12;
		TestOutput("NVARCHAR(20)", "foo test", SQL_C_CHAR, SQL_WVARCHAR, "8 foo test");
		/* TODO use collate in sintax if available */
		TestOutput("NVARCHAR(20)", "0xf800f900", SQL_C_CHAR, SQL_WVARCHAR, "2 \xf8\xf9");

		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WVARCHAR, "NVARCHAR(10)", "1EasyTest2");
		use_nts = 1;
		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_WVARCHAR, "NVARCHAR(10)", "1EasyTest3");
		TestInput(SQL_C_WCHAR, "NVARCHAR(3)", SQL_WVARCHAR, "NVARCHAR(3)", "0xf800a300bc06");

		TestInput(SQL_C_WCHAR, "NVARCHAR(10)", SQL_INTEGER, "INT", " -423785  -> -423785");
	}
}

int
main(int argc, char *argv[])
{
	SQLSMALLINT version_len;

	use_odbc_version3 = 1;
	Connect();

	memset(version, 0, sizeof(version));
	SQLGetInfo(Connection, SQL_DBMS_VER, version, sizeof(version), &version_len);

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;

	for (use_cursors = 0; use_cursors <= 1; ++use_cursors) {
		if (use_cursors) {
			if (!tds_no_dm || !driver_is_freetds())
				ResetStatement();
			CheckCursor();
		}

		exec_direct = 1;
		AllTests();

		exec_direct = 0;
		prepare_before = 1;
		AllTests();

		prepare_before = 0;
		AllTests();
	}

	Disconnect();

	printf("Done successfully!\n");
	return 0;
}

