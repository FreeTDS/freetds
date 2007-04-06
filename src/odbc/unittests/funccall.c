#include "common.h"

/* Test for {?=call store(?)} syntax and run */

static char software_version[] = "$Id: funccall.c,v 1.13 2007-04-06 08:31:05 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER input, output;
	SQLLEN ind, ind2, ind3, ind4;
	SQLINTEGER out1;
	char out2[30];

	Connect();

	if (CommandWithResult(Statement, "drop proc simpleresult") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	Command(Statement, "create proc simpleresult @i int as begin return @i end");

	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter");

	if (SQLPrepare(Statement, (SQLCHAR *) "{ \n?\t\r= call simpleresult(?)}", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	input = 123;
	ind2 = sizeof(input);
	output = 0xdeadbeef;
	if (SQLExecute(Statement) != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
		exit(1);
	}

	if (output != 123) {
		printf("Invalid result\n");
		exit(1);
	}

	/* should return "Invalid cursor state" */
	if (SQLFetch(Statement) != SQL_ERROR) {
		printf("Data not expected\n");
		exit(1);
	}

	/* just to reset some possible buffers */
	Command(Statement, "DECLARE @i INT");

	/* same test but with SQLExecDirect and same bindings */
	input = 567;
	ind2 = sizeof(input);
	output = 0xdeadbeef;
	if (SQLExecDirect(Statement, (SQLCHAR *) "{?=call simpleresult(?)}", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to execure direct statement\n");
		exit(1);
	}

	if (output != 567) {
		printf("Invalid result\n");
		exit(1);
	}

	/* should return "Invalid cursor state" */
	if (SQLFetch(Statement) != SQL_ERROR) {
		printf("Data not expected\n");
		exit(1);
	}

	Command(Statement, "drop proc simpleresult");

	if (CommandWithResult(Statement, "drop proc simpleresult2") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	/* force cursor close */
	SQLCloseCursor(Statement);

	/* test output parameter */
	Command(Statement,
		"create proc simpleresult2 @i int, @x int output, @y varchar(20) output as begin select @x = 6789 select @y = 'test foo' return @i end");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind) != SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}

	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind2) != SQL_SUCCESS) {
		printf("Unable to bind input parameter\n");
		exit(1);
	}

	if (SQLBindParameter(Statement, 3, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &out1, 0, &ind3) != SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}

	if (SQLBindParameter(Statement, 4, SQL_PARAM_OUTPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, out2, sizeof(out2), &ind4) !=
	    SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}

	if (SQLPrepare(Statement, (SQLCHAR *) "{ \n?\t\r= call simpleresult2(?,?,?)}", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	input = 987;
	ind2 = sizeof(input);
	out1 = 888;
	output = 0xdeadbeef;
	ind3 = SQL_DATA_AT_EXEC;
	ind4 = SQL_DEFAULT_PARAM;
	if (SQLExecute(Statement) != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
		exit(1);
	}

	if (output != 987 || ind3 <= 0 || ind4 <= 0 || out1 != 6789 || strcmp(out2, "test foo") != 0) {
		printf("ouput = %d ind3 = %d ind4 = %d out1 = %d out2 = %s\n", (int) output, (int) ind3, (int) ind4, (int) out1,
		       out2);
		printf("Invalid result\n");
		exit(1);
	}

	/* should return "Invalid cursor state" */
	if (SQLFetch(Statement) != SQL_ERROR) {
		printf("Data not expected\n");
		exit(1);
	}

	Command(Statement, "drop proc simpleresult2");

	/*
	 * test from shiv kumar
	 * Cfr ML 2006-11-21 "specifying a 0 for the StrLen_or_IndPtr in the
	 * SQLBindParameter call is not working on AIX"
	 */
	if (CommandWithResult(Statement, "drop proc rpc_read") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	SQLCloseCursor(Statement);

	Command(Statement, "create proc rpc_read @i int, @x timestamp as begin select 1 return 1234 end");
	SQLCloseCursor(Statement);
	SQLFreeStmt(Statement, SQL_CLOSE);
	SQLFreeStmt(Statement, SQL_UNBIND);

	if (SQLPrepare(Statement, (SQLCHAR *) "{ ? = CALL rpc_read ( ?, ? ) }" , SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement\n");

	ind = 0;
	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &output, 0, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter\n");

	ind2 = 0;
	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &input, 0, &ind2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter\n");

	ind3 = 8;
	if (SQLBindParameter(Statement, 3, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, 8, 0, out2, 8, &ind3) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter\n");

	if (SQLExecute(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute statement\n");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Data not expected\n");

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected\n");

	Disconnect();

	printf("Done.\n");
	return 0;
}
