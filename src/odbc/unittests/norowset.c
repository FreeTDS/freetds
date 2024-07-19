#include "common.h"

/* Test that a select following a store procedure execution return results */

TEST_MAIN()
{
	char output[256];
	SQLLEN dataSize;

	odbc_connect();

	odbc_command_with_result(odbc_stmt, "drop proc sp_norowset_test");

	odbc_command("create proc sp_norowset_test as begin declare @i int end");

	odbc_command("exec sp_norowset_test");

	/* note, mssql 2005 seems to not return row for tempdb, use always master */
	odbc_command("select name from master..sysobjects where name = 'sysobjects'");
	CHKFetch("S");

	CHKGetData(1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");

	if (strcmp(output, "sysobjects") != 0) {
		printf("Unexpected result\n");
		exit(1);
	}

	CHKFetch("No");

	CHKMoreResults("No");

	odbc_command("drop proc sp_norowset_test");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
