#include "common.h"

/* Test for SQLMoreResults and SQLRowCount on batch */

static char software_version[] = "$Id: moreandcount.c,v 1.2 2003-05-07 12:48:56 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
CheckRows(int n)
{
	SQLINTEGER rows;
	SQLRETURN res;

	res = SQLRowCount(Statement, &rows);
	if (res != SQL_SUCCESS) {
		if (res == SQL_ERROR && n < -1)
			return;
		fprintf(stderr, "Unable to get row\n");
		CheckReturn();
		exit(1);
	}

	if (rows != n) {
		fprintf(stderr, "Expected %d rows returned %d\n", n, (int) rows);
		exit(1);
	}
}

static void
NextResults(SQLRETURN expected)
{
	if (SQLMoreResults(Statement) != expected) {
		if (expected == SQL_SUCCESS)
			fprintf(stderr, "Expected another recordset\n");
		else
			fprintf(stderr, "Not expected another recordset\n");
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

int
main(int argc, char *argv[])
{
	Connect();

	Command(Statement, "create table #tmp1 (i int)");
	Command(Statement, "create table #tmp2 (i int)");
	Command(Statement, "insert into #tmp1 values(1)");
	Command(Statement, "insert into #tmp1 values(2)");
	Command(Statement, "insert into #tmp1 values(3)");

	/* execute a batch command select insert insert select and check rows */
	if (SQLExecDirect(Statement, (SQLCHAR *) "SELECT * FROM #tmp1 WHERE i <= 2 "
			  "INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i = 1 "
			  "INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i <= 2 "
			  "SELECT * FROM #tmp1 WHERE i = 1", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to execure direct statement\n");
		return 1;
	}

	printf("Result 1\n");
	CheckRows(-1);

	Fetch(SQL_SUCCESS);
	Fetch(SQL_SUCCESS);
	CheckRows(-1);
	Fetch(SQL_NO_DATA);
	CheckRows(2);

	NextResults(SQL_SUCCESS);

	printf("Result 2\n");
	CheckRows(1);

	NextResults(SQL_SUCCESS);

	printf("Result 3\n");
	CheckRows(2);

	NextResults(SQL_SUCCESS);

	printf("Result 4\n");
	CheckRows(-1);
	Fetch(SQL_SUCCESS);
	CheckRows(-1);
	Fetch(SQL_NO_DATA);
	CheckRows(1);

	NextResults(SQL_NO_DATA);

	CheckRows(-2);

	Disconnect();

	printf("Done.\n");
	return 0;
}
