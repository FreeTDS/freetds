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


static char software_version[] = "$Id: norowset.c,v 1.1 2003-02-14 13:19:19 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* Test that a select following a store procedure execution return results */

int
main(int argc, char *argv[])
{
	int res;
	char output[256];
	const char *command;
	SQLINTEGER dataSize;

	Connect();

	command = "drop proc sp_norowset_test";
	printf("%s\n", command);
	SQLExecDirect(Statement, (SQLCHAR *) command, SQL_NTS);

	Command(Statement, "create proc sp_norowset_test as begin declare @i int end");

	Command(Statement, "exec sp_norowset_test");

	Command(Statement, "select name from sysobjects where name = 'sysobjects'");
	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, output, sizeof(output), &dataSize) != SQL_SUCCESS) {
		printf("Unable to get data col %d\n", 1);
		CheckReturn();
		exit(1);
	}

	if (strcmp(output, "sysobjects") != 0) {
		printf("Unexpected result\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_NO_DATA) {
		printf("Row not expected\n");
		CheckReturn();
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	Command(Statement, "drop proc sp_norowset_test");

	Disconnect();

	printf("Done.\n");
	return 0;
}
