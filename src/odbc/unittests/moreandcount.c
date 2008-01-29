#include "common.h"

/* Test for SQLMoreResults and SQLRowCount on batch */

static char software_version[] = "$Id: moreandcount.c,v 1.15 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
NextResults(SQLRETURN expected, int line)
{
	if (SQLMoreResults(Statement) != expected) {
		if (expected == SQL_SUCCESS)
			fprintf(stderr, "Expected another recordset line %d\n", line);
		else
			fprintf(stderr, "Not expected another recordset line %d\n", line);
		exit(1);
	}
}

static void
Fetch(SQLRETURN expected)
{
	if (SQLFetch(Statement) != expected) {
		if (expected == SQL_SUCCESS)
			fprintf(stderr, "Expected another record\n");
		else
			fprintf(stderr, "Not expected another record\n");
		exit(1);
	}
}

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
		CHK(SQLPrepare, (Statement, (SQLCHAR *) query, SQL_NTS));
		CHK(SQLExecute, (Statement));
	} else {

		/* execute a batch command select insert insert select and check rows */
		CHK(SQLExecDirect, (Statement, (SQLCHAR *) query, SQL_NTS));
	}
	if (!prepare) {
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(1);
		NextResults(SQL_SUCCESS, __LINE__);
	}
	printf("Result %d\n", ++n);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	Fetch(SQL_SUCCESS);
	Fetch(SQL_SUCCESS);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	Fetch(SQL_NO_DATA);
	CHECK_COLS(1);
	CHECK_ROWS(2);
	NextResults(SQL_SUCCESS, __LINE__);
	if (!prepare) {
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(1);
		NextResults(SQL_SUCCESS, __LINE__);
		printf("Result %d\n", ++n);
		CHECK_COLS(0);
		CHECK_ROWS(2);
		NextResults(SQL_SUCCESS, __LINE__);
	}
	printf("Result %d\n", ++n);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	Fetch(SQL_SUCCESS);
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	Fetch(SQL_NO_DATA);
	CHECK_COLS(1);
	if (prepare) {
		/* collapse 2 recordset... after a lot of testing this is the behavior! */
		CHECK_ROWS(2);
	} else {
		CHECK_ROWS(1);
		NextResults(SQL_SUCCESS, __LINE__);
		CHECK_COLS(0);
		CHECK_ROWS(2);
	}

	NextResults(SQL_NO_DATA, __LINE__);
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

	Command(Statement, "create table #tmp1 (i int)");
	Command(Statement, "create table #tmp2 (i int)");
	Command(Statement, "insert into #tmp1 values(1)");
	Command(Statement, "insert into #tmp1 values(2)");
	Command(Statement, "insert into #tmp1 values(5)");

	printf("Use direct statement\n");
	DoTest(0);

	printf("Use prepared statement\n");
	DoTest(1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
