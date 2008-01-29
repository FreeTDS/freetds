#include "common.h"

/* Test for {?=call store(?)} syntax and run */

static char software_version[] = "$Id: funccall.c,v 1.15 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER input, output;
	SQLLEN ind, ind2, ind3, ind4;
	SQLINTEGER out1;
	char out2[30];

	Connect();

	Command(Statement, "IF OBJECT_ID('simpleresult') IS NOT NULL DROP PROC simpleresult");

	Command(Statement, "create proc simpleresult @i int as begin return @i end");

	CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind2));

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{ \n?\t\r= call simpleresult(?)}", SQL_NTS));

	input = 123;
	ind2 = sizeof(input);
	output = 0xdeadbeef;
	CHK(SQLExecute, (Statement));

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
	CHK(SQLExecDirect, (Statement, (SQLCHAR *) "{?=call simpleresult(?)}", SQL_NTS));

	if (output != 567) {
		fprintf(stderr, "Invalid result\n");
		exit(1);
	}

	/* should return "Invalid cursor state" */
	if (SQLFetch(Statement) != SQL_ERROR) {
		fprintf(stderr, "Data not expected\n");
		exit(1);
	}

	Command(Statement, "drop proc simpleresult");

	Command(Statement, "IF OBJECT_ID('simpleresult2') IS NOT NULL DROP PROC simpleresult2");

	/* force cursor close */
	SQLCloseCursor(Statement);

	/* test output parameter */
	Command(Statement,
		"create proc simpleresult2 @i int, @x int output, @y varchar(20) output as begin select @x = 6789 select @y = 'test foo' return @i end");

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0,  0, &output, 0,            &ind));
	CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_INPUT,  SQL_C_SLONG, SQL_INTEGER, 0,  0, &input,  0,            &ind2));
	CHK(SQLBindParameter, (Statement, 3, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0,  0, &out1,   0,            &ind3));
	CHK(SQLBindParameter, (Statement, 4, SQL_PARAM_OUTPUT, SQL_C_CHAR,  SQL_VARCHAR, 20, 0, out2,    sizeof(out2), &ind4));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{ \n?\t\r= call simpleresult2(?,?,?)}", SQL_NTS));

	input = 987;
	ind2 = sizeof(input);
	out1 = 888;
	output = 0xdeadbeef;
	ind3 = SQL_DATA_AT_EXEC;
	ind4 = SQL_DEFAULT_PARAM;
	CHK(SQLExecute, (Statement));

	if (output != 987 || ind3 <= 0 || ind4 <= 0 || out1 != 6789 || strcmp(out2, "test foo") != 0) {
		printf("ouput = %d ind3 = %d ind4 = %d out1 = %d out2 = %s\n", (int) output, (int) ind3, (int) ind4, (int) out1,
		       out2);
		printf("Invalid result\n");
		exit(1);
	}

	/* should return "Invalid cursor state" */
	if (SQLFetch(Statement) != SQL_ERROR) {
		fprintf(stderr, "Data not expected\n");
		exit(1);
	}

	Command(Statement, "drop proc simpleresult2");

	/*
	 * test from shiv kumar
	 * Cfr ML 2006-11-21 "specifying a 0 for the StrLen_or_IndPtr in the
	 * SQLBindParameter call is not working on AIX"
	 */
	Command(Statement, "IF OBJECT_ID('rpc_read') IS NOT NULL DROP PROC rpc_read");

	ResetStatement();

	Command(Statement, "create proc rpc_read @i int, @x timestamp as begin select 1 return 1234 end");
	SQLCloseCursor(Statement);

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{ ? = CALL rpc_read ( ?, ? ) }" , SQL_NTS));

	ind = 0;
	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &output, 0, &ind));

	ind2 = 0;
	CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &input, 0, &ind2));

	ind3 = 8;
	CHK(SQLBindParameter, (Statement, 3, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, 8, 0, out2, 8, &ind3));

	CHK(SQLExecute, (Statement));

	CHK(SQLFetch, (Statement));

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected\n");

	ResetStatement();
	Command(Statement, "drop proc rpc_read");

	/*
	 * Test from Joao Amaral
	 * This test SQLExecute where a store procedure returns no result
	 * This seems similar to a previous one but use set instead of select
	 * (with is supported only by mssql and do not return all stuff as 
	 * select does)
	 */
	if (db_is_microsoft()) {

		ResetStatement();

		Command(Statement, "IF OBJECT_ID('sp_test') IS NOT NULL DROP PROC sp_test");
		Command(Statement, "create proc sp_test @res int output as set @res = 456");

		ResetStatement();

		CHK(SQLPrepare, (Statement, (SQLCHAR *) "{ call sp_test(?)}", SQL_NTS));
		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind));

		output = 0xdeadbeef;
		CHK(SQLExecute, (Statement));

		if (output != 456) {
			fprintf(stderr, "Invalid result %d(%x)\n", (int) output, (int) output);
			return 1;
		}
		Command(Statement, "drop proc sp_test");
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
