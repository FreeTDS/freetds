#include "common.h"

/* Test for ODBC Rowcount when doing an update with trigger */

/* avoid indentation using GNU indent */
#define STR(s) s

TEST_MAIN()
{
	SQLTCHAR *query = T("update test_update set type=type");

	odbc_use_version3 = false;
	odbc_connect();

	/* Cannot use temp table here since they don't allow triggers */
	odbc_command("IF OBJECT_ID('test_update') IS NOT NULL DROP TABLE test_update");
	odbc_command("create table test_update (type char(1))");
	odbc_command("insert test_update values ('A')");
	odbc_command("insert test_update values ('B')");
	odbc_command("insert test_update values ('C')");
	odbc_command("insert test_update values ('D')");
	odbc_command("insert test_update values ('E')");
	odbc_command(STR("create trigger test_foo\n")
		     STR("on test_update for update as\n")
		     STR("declare @v int\n")
		     STR("select @v=137"));

	puts("SQLPrepare+SQLExecute with trigger");
	CHKPrepare(query, SQL_NTS, "S");
	CHKExecute("S");
	ODBC_CHECK_ROWS(5);

	puts("SQLExecDirect with trigger");
	CHKExecDirect(query, SQL_NTS, "S");
	ODBC_CHECK_ROWS(5);

	CHKExecDirect(T("drop trigger test_foo"), SQL_NTS, "S");

	puts("SQLPrepare+SQLExecute without trigger");
	CHKPrepare(query, SQL_NTS, "S");
	CHKExecute("S");
	ODBC_CHECK_ROWS(5);

	puts("SQLExecDirect without trigger");
	CHKExecDirect(query, SQL_NTS, "S");
	ODBC_CHECK_ROWS(5);

	/* Cleanup */
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
