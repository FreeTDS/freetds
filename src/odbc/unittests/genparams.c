#include "common.h"
#include <assert.h>
#include <time.h>

/* Test various type from odbc and to odbc */

static char software_version[] = "$Id: genparams.c,v 1.21 2008-01-06 10:48:45 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int precision = 18;
static int exec_direct = 0;
static int prepare_before = 0;
static int use_cursors = 0;

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;

	ResetStatement();

	/* build store procedure to test */
	Command(Statement, "IF OBJECT_ID('spTestProc') IS NOT NULL DROP PROC spTestProc");
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, '%s')", type, type, value_to_convert);
	Command(Statement, sbuf);
	memset(out_buf, 0, sizeof(out_buf));

	if (use_cursors) {
		ResetStatement();
		if (SQLSetStmtAttr(Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLSetStmtAttr error");
		if (SQLSetStmtAttr(Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLSetStmtAttr error");
	}

	/* bind parameter */
	if (exec_direct) {
		if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to bind output parameter");

		/* call store procedure */
		if (SQLExecDirect(Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to execute store statement");
	} else {
		if (prepare_before && SQLPrepare(Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare() failure!");

		if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, precision, 0, out_buf,
			     sizeof(out_buf), &out_len) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to bind output parameter");

		if (!prepare_before && SQLPrepare(Statement, (SQLCHAR *) "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare() failure!");

		if (SQLExecute(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLExecute() failure!");
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

static void
TestInput(SQLSMALLINT out_c_type, const char *type, SQLSMALLINT out_sql_type, const char *param_type, const char *value_to_convert)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	const char *expected = value_to_convert;
	size_t value_len = strlen(value_to_convert);
	const char *p;

	ResetStatement();

	/* execute a select to get data as wire */
	if ((p = strstr(value_to_convert, " -> ")) != NULL) {
		value_len = p - value_to_convert;
		expected = p + 4;
	}
	sprintf(sbuf, "SELECT CONVERT(%s, '%.*s')", type, (int) value_len, value_to_convert);
	Command(Statement, sbuf);
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		ODBC_REPORT_ERROR("Expected row");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Row not expected");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Recordset not expected");

	/* create a table with a column of that type */
	ResetStatement();
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s)", param_type);
	Command(Statement, sbuf);

	if (use_cursors) {
		ResetStatement();
		if (SQLSetStmtAttr(Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLSetStmtAttr error");
		if (SQLSetStmtAttr(Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLSetStmtAttr error");
	}

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (exec_direct) {
		SQLRETURN rc;

		out_len = 1;
		if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len) !=
		    SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to bind input parameter");

		rc = SQLExecDirect(Statement, (SQLCHAR *) sbuf, SQL_NTS);
		if (rc != SQL_SUCCESS && rc != SQL_NO_DATA)
			ODBC_REPORT_ERROR("SQLExecDirect() failure!");
	} else {
		SQLRETURN rc;

		if (prepare_before && SQLPrepare(Statement, (SQLCHAR *) sbuf, SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare() failure!");

		out_len = 1;
		if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len) !=
		    SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to bind input parameter");

		if (!prepare_before && SQLPrepare(Statement, (SQLCHAR *) sbuf, SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare() failure!");

		rc = SQLExecute(Statement);
		if (rc != SQL_SUCCESS && rc != SQL_NO_DATA)
			ODBC_REPORT_ERROR("SQLExecute() failure!");
	}

	/* check is row is present */
	ResetStatement();
	sprintf(sbuf, "SELECT * FROM #tmp_insert WHERE col = CONVERT(%s, '%s')", param_type, expected);
	Command(Statement, sbuf);

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expected row");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Row not expected");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Recordset not expected");
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
	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");
	Test("DECIMAL(18,2)", "123", SQL_C_NUMERIC, SQL_DECIMAL, "18 0 1 7B");
	precision = 38;
	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "38 0 1 7B");
	TestInput(SQL_C_LONG, "INTEGER", SQL_VARCHAR, "VARCHAR(20)", "12345");
	/* MS driver behavior for output parameters is different */
	if (driver_is_freetds())
		Test("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "333133323333");
	else
		Test("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "313233");
	/* FIXME our driver ignore precision for date */
	precision = 3;
	Test("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, big_endian ? "0000949700FBAA2C" : "979400002CAAFB00");
	Test("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP,
	     big_endian ? "0000949700FB9640" : "979400004096FB00");
	TestInput(SQL_C_TYPE_TIMESTAMP, "DATETIME", SQL_TYPE_TIMESTAMP, "DATETIME", "2005-07-22 09:51:34");
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

	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "0");
	TestInput(SQL_C_BIT, "BIT", SQL_BIT, "BIT", "1");

	TestInput(SQL_C_DOUBLE, "MONEY", SQL_DOUBLE, "MONEY", "123.34");

	/* TODO some Sybase versions */
	if (db_is_microsoft() && strncmp(version, "08.00.", 6) == 0) {
		Test("BIGINT", "-987654321065432", SQL_C_BINARY, SQL_BIGINT, big_endian ? "FFFC7DBBCF083228" : "283208CFBB7DFCFF");
		TestInput(SQL_C_SBIGINT, "BIGINT", SQL_BIGINT, "BIGINT", "-12345678901234");
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
		if (use_cursors)
			CheckCursor();

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

