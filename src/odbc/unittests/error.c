#include "common.h"

/* some tests on error reporting */

TEST_MAIN()
{
	SQLRETURN RetCode;
	HSTMT stmt;

	odbc_connect();

	/* create a test table */
	odbc_command("create table #tmp (i int)");
	odbc_command("insert into #tmp values(3)");
	odbc_command("insert into #tmp values(4)");
	odbc_command("insert into #tmp values(5)");
	odbc_command("insert into #tmp values(6)");
	odbc_command("insert into #tmp values(7)");

	odbc_command("create table #names(vc varchar(100))");
	odbc_command("set nocount on\ndeclare @i int\nselect @i = 1\n"
		     "while @i <= 1000 begin\n"
		     "  insert into #names values('this is a different name ' + convert(varchar(10), @i))\n"
		     "  select @i = @i + 1\nend\nset nocount off\n");

	/* issue our command */
	RetCode = odbc_command2("select 100 / (i - 5) from #tmp order by i", "SE");

	/* special case, early Sybase detect error early */
	if (RetCode != SQL_ERROR) {

		/* TODO when multiple row fetch available test for error on some columns */
		CHKFetch("S");
		CHKFetch("S");
		CHKFetch("E");
	}

	odbc_read_error();
	if (!strstr(odbc_err, "zero")) {
		fprintf(stderr, "Message invalid\n");
		return 1;
	}

	SQLFetch(odbc_stmt);
	SQLFetch(odbc_stmt);
	SQLFetch(odbc_stmt);
	SQLMoreResults(odbc_stmt);

	CHKAllocStmt(&stmt, "S");

	odbc_command("SELECT * FROM #names");

	odbc_stmt = stmt;

	/* a statement is already active so you get error... */
	if (odbc_command2("SELECT * FROM #names", "SE") == SQL_SUCCESS) {
		SQLMoreResults(odbc_stmt);
		/* ...or we are using MARS! */
		odbc_command2("BEGIN TRANSACTION", "E");
	}

	odbc_read_error();

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
