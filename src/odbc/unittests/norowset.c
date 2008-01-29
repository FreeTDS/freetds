#include "common.h"

static char software_version[] = "$Id: norowset.c,v 1.6 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* Test that a select following a store procedure execution return results */

int
main(int argc, char *argv[])
{
	int res;
	char output[256];
	SQLLEN dataSize;

	Connect();

	CommandWithResult(Statement, "drop proc sp_norowset_test");

	Command(Statement, "create proc sp_norowset_test as begin declare @i int end");

	Command(Statement, "exec sp_norowset_test");

	/* note, mssql 2005 seems to not return row for tempdb, use always master */
	Command(Statement, "select name from master..sysobjects where name = 'sysobjects'");
	CHK(SQLFetch, (Statement));

	CHK(SQLGetData, (Statement, 1, SQL_C_CHAR, output, sizeof(output), &dataSize));

	if (strcmp(output, "sysobjects") != 0) {
		printf("Unexpected result\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_NO_DATA) {
		fprintf(stderr, "Row not expected\n");
		CheckReturn();
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Not expected another recordset\n");
		exit(1);
	}

	Command(Statement, "drop proc sp_norowset_test");

	Disconnect();

	printf("Done.\n");
	return 0;
}
