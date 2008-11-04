#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0003.c,v 1.18 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int prepared)
{
	Command(Statement, "create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		CHKPrepare((SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS, "S");
		CHKExecute("S");
	}

	CHKFetch("No");

	CHKMoreResults("S");
	printf("Getting next recordset\n");

	CHKFetch("No");

	CHKMoreResults("No");

	/* test that skipping a no empty result go to other result set */
	Command(Statement, "insert into #odbctestdata values(123)");
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		CHKPrepare((SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS, "S");
		CHKExecute("S");
	}

	CHKMoreResults("S");
	printf("Getting next recordset\n");

	CHKFetch("S");

	CHKFetch("No");

	CHKMoreResults("No");

	Command(Statement, "drop table #odbctestdata");
}

int
main(int argc, char *argv[])
{
	Connect();

	DoTest(0);
	DoTest(1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
