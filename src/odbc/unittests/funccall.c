#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "common.h"

/* Test for {?=call store(?)} syntax and run */

static char software_version[] = "$Id: funccall.c,v 1.1 2003-02-27 15:06:05 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[128];
	SQLINTEGER ind, ind2, output;
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

	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, buf, 128, &ind2) != SQL_SUCCESS) {
		printf("Unable to bind input parameter\n");
		exit(1);
	}

	if (SQLPrepare(Statement, (SQLCHAR *) "{?=call simpleresult(?)}", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	strcpy(buf, "123");
	ind2 = SQL_NTS;
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

	Command(Statement, "drop proc simpleresult");

	Disconnect();

	printf("Done.\n");
	return 0;
}
