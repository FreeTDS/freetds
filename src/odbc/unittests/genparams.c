#include "common.h"
#include <assert.h>

/* Test various type from odbc and to odbc */

static char software_version[] = "$Id: genparams.c,v 1.9 2004-10-28 13:16:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* build store procedure to test */
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, '%s')", type, type, value_to_convert);
	Command(Statement, sbuf);
	memset(out_buf, 0, sizeof(out_buf));

	/* bind parameter */
	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, 18, 0, out_buf, sizeof(out_buf), &out_len) !=
	    SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter");

	/* call store procedure */
	if (SQLExecDirect(Statement, "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute store statement");

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

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* execute a select to get data as wire */
	sprintf(sbuf, "SELECT CONVERT(%s, '%s')", type, value_to_convert);
	Command(Statement, sbuf);
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expected row");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Row not expected");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Recordset not expected");

	/* create a table with a column of that type */
	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s)", param_type);
	Command(Statement, sbuf);

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (SQLPrepare(Statement, sbuf, SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLPrepare() failure!");

	out_len = 1;
	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len) !=
	    SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter");

	if (SQLExecute(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLExecute() failure!");

	/* check is row is present */
	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);
	sprintf(sbuf, "SELECT * FROM #tmp_insert WHERE col = CONVERT(%s, '%s')", param_type, value_to_convert);
	Command(Statement, sbuf);

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expected row");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Row not expected");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Recordset not expected");
	Command(Statement, "DROP TABLE #tmp_insert");
}

int
main(int argc, char *argv[])
{
	int big_endian = 1;

	Connect();

	if (CommandWithResult(Statement, "drop proc spTestProc") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;

	/* FIXME why should return 38 0 as precision and scale ?? correct ?? */
	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");
	TestInput(SQL_C_LONG, "INTEGER", SQL_VARCHAR, "VARCHAR(20)", "12345");
	/* MS driver behavior for output parameters is different */
	if (driver_is_freetds())
		Test("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "333133323333");
	else
		Test("VARCHAR(20)", "313233", SQL_C_BINARY, SQL_VARCHAR, "313233");
	Test("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP, big_endian ? "0000949700FBAA2C" : "979400002CAAFB00");
	Test("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, SQL_TIMESTAMP,
	     big_endian ? "0000949700FB9640" : "979400004096FB00");

	Disconnect();

	printf("Done successfully!\n");
	return 0;
}
