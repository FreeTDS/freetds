#include "common.h"

/* Test various type from odbc and to odbc */

static char software_version[] = "$Id: genparams.c,v 1.3 2003-12-06 16:35:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLINTEGER out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* build store procedure to test */
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, '%s')", type, type, value_to_convert);
	Command(Statement, sbuf);

	/* bind parameter */
	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, 18, 0, out_buf, sizeof(out_buf), &out_len) !=
	    SQL_SUCCESS) {
		fprintf(stderr, "Unable to bind input parameter\n");
		CheckReturn();
	}

	/* call store procedure */
	if (SQLExecDirect(Statement, "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to execute store statement\n");
		CheckReturn();
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
	SQLINTEGER out_len = 0;

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* execute a select to get data as wire */
	sprintf(sbuf, "SELECT CONVERT(%s, '%s')", type, value_to_convert);
	Command(Statement, sbuf);
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(SQLINTEGER), &out_len);
	if (SQLFetch(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "Expected row\n");
		exit(1);
	}
	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Row not expected\n");
		exit(1);
	}
	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Recordset not expected\n");
		exit(1);
	}

	/* create a table with a column of that type */
	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);
	sprintf(sbuf, "CREATE TABLE #tmp_insert (col %s)", param_type);
	Command(Statement, sbuf);

	/* insert data using prepared statements */
	sprintf(sbuf, "INSERT INTO #tmp_insert VALUES(?)");
	if (SQLPrepare(Statement, sbuf, SQL_NTS) != SQL_SUCCESS) {
		fprintf(stderr, "SQLPrepare() failure!\n");
		exit(1);
	}

	out_len = 1;
	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, out_c_type, out_sql_type, 20, 0, out_buf, sizeof(out_buf), &out_len) !=
	    SQL_SUCCESS) {
		fprintf(stderr, "Unable to bind input parameter\n");
		CheckReturn();
	}

	if (SQLExecute(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "SQLExecute() failure!\n");
		exit(1);
	}

	/* check is row is present */
	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);
	sprintf(sbuf, "SELECT * FROM #tmp_insert WHERE col = CONVERT(%s, '%s')", param_type, value_to_convert);
	Command(Statement, sbuf);

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "Expected row\n");
		exit(1);
	}
	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Row not expected\n");
		exit(1);
	}
	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Recordset not expected\n");
		exit(1);
	}
	Command(Statement, "DROP TABLE #tmp_insert");
}

int
main(int argc, char *argv[])
{
	Connect();

	if (CommandWithResult(Statement, "drop proc spTestProc") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	/* FIXME why should return 38 0 as precision and scale ?? correct ?? */
	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");
	TestInput(SQL_C_LONG, "INTEGER", SQL_VARCHAR, "VARCHAR(20)", "12345");

	Disconnect();

	printf("Done successfully!\n");
	return 0;
}
