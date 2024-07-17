#include "common.h"

/* Test for SQLMoreResults */

static void
DoTest(int prepared)
{
	odbc_command("create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	if (!prepared) {
		odbc_command("select * from #odbctestdata select * from #odbctestdata");
	} else {
		CHKPrepare(T("select * from #odbctestdata select * from #odbctestdata"), SQL_NTS, "S");
		CHKExecute("S");
	}

	CHKFetch("No");

	CHKMoreResults("S");
	printf("Getting next recordset\n");

	CHKFetch("No");

	CHKMoreResults("No");

	/* test that skipping a no empty result go to other result set */
	odbc_command("insert into #odbctestdata values(123)");
	if (!prepared) {
		odbc_command("select * from #odbctestdata select * from #odbctestdata");
	} else {
		CHKPrepare(T("select * from #odbctestdata select * from #odbctestdata"), SQL_NTS, "S");
		CHKExecute("S");
	}

	CHKMoreResults("S");
	printf("Getting next recordset\n");

	CHKFetch("S");

	CHKFetch("No");

	CHKMoreResults("No");

	odbc_command("drop table #odbctestdata");

	ODBC_FREE();
}

TEST_MAIN()
{
	odbc_connect();

	DoTest(0);
	DoTest(1);

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
