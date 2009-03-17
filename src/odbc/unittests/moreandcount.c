#include "common.h"

/* Test for SQLMoreResults and SQLRowCount on batch */

static char software_version[] = "$Id: moreandcount.c,v 1.18 2009-03-17 09:05:47 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int prepare)
{
	int n = 0;
	static const char query[] =
		/* on prepared this recordset should be skipped */
		"DECLARE @b INT SELECT @b = 25 "
		"SELECT * FROM #tmp1 WHERE i <= 3 "
		/* on prepare we cannot distinguish these recordset */
		"INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i = 1 "
		"INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i <= 3 "
		"SELECT * FROM #tmp1 WHERE i = 1 "
		/* but FreeTDS can detect last recordset */
		"UPDATE #tmp1 SET i=i+1 WHERE i >= 2";

	if (prepare) {
		CHKPrepare((SQLCHAR *) query, SQL_NTS, "S");
		CHKExecute("S");
	} else {

		/* execute a batch command select insert insert select and check rows */
		CHKExecDirect((SQLCHAR *) query, SQL_NTS, "S");
	}
	if (!prepare) {
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(1);
		CHKMoreResults("S");
	}
	printf("Result %d\n", ++n);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	CHKFetch("S");
	CHKFetch("S");
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	CHKFetch("No");
	CHECK_COLS(1);
	CHECK_ROWS(2);
	CHKMoreResults("S");
	if (!prepare) {
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(1);
		CHKMoreResults("S");
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(2);
		CHKMoreResults("S");
	}
	printf("Result %d\n", ++n);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	CHKFetch("S");
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	CHKFetch("No");
	CHECK_COLS(1);
	if (prepare) {
		/* collapse 2 recordset... after a lot of testing this is the behavior! */
		if (driver_is_freetds())
			CHECK_ROWS(2);
	} else {
		CHECK_ROWS(1);
		CHKMoreResults("S");
		CHECK_COLS(0);
		CHECK_ROWS(2);
	}

	CHKMoreResults("No");
#ifndef TDS_NO_DM
	if (!prepare)
		CHECK_COLS(-1);
	CHECK_ROWS(-2);
#endif
}

int
main(int argc, char *argv[])
{
	Connect();

	Command("create table #tmp1 (i int)");
	Command("create table #tmp2 (i int)");
	Command("insert into #tmp1 values(1)");
	Command("insert into #tmp1 values(2)");
	Command("insert into #tmp1 values(5)");

	printf("Use direct statement\n");
	DoTest(0);

	printf("Use prepared statement\n");
	DoTest(1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
