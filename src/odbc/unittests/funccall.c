#include "common.h"

/* Test for {?=call store(?)} syntax and run */

static char software_version[] = "$Id: funccall.c,v 1.3 2003-04-01 12:01:35 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER input, ind, ind2, output;
	const char *command;

	Connect();

	command = "drop proc simpleresult";
	printf("%s\n", command);
	if (SQLExecDirect(Statement, (SQLCHAR *) command, SQL_NTS)
	    != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
	}

	Command(Statement, "create proc simpleresult @i int as begin return @i end");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind) != SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}

	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind2) != SQL_SUCCESS) {
		printf("Unable to bind input parameter\n");
		exit(1);
	}

	if (SQLPrepare(Statement, (SQLCHAR *) "{?=call simpleresult(?)}", SQL_NTS) != SQL_SUCCESS) {
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

	Disconnect();

	printf("Done.\n");
	return 0;
}
