#include "common.h"

/* Test for SQLMoreResults and SQLRowCount on batch */

static char software_version[] = "$Id: moreandcount.c,v 1.7 2004-02-14 19:39:11 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
CheckCols(int n)
{
	SQLSMALLINT cols;
	SQLRETURN res;

	res = SQLNumResultCols(Statement, &cols);
	if (res != SQL_SUCCESS) {
		if (res == SQL_ERROR && n < 0)
			return;
		fprintf(stderr, "Unable to get column numbers\n");
		CheckReturn();
		exit(1);
	}

	if (cols != n) {
		fprintf(stderr, "Expected %d columns returned %d\n", n, (int) cols);
		exit(1);
	}
}

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

static void
DoTest(int prepare)
{
	int n = 0;
	static const char query[] = "DECLARE @b INT SELECT @b = 25 "
		"SELECT * FROM #tmp1 WHERE i <= 3 "
		"INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i = 1 "
		"INSERT INTO #tmp2 SELECT * FROM #tmp1 WHERE i <= 3 "
		"SELECT * FROM #tmp1 WHERE i = 1 "
		"UPDATE #tmp1 SET i=i+1 WHERE i >= 2";

	if (prepare) {
		if (SQLPrepare(Statement, (SQLCHAR *) query, SQL_NTS) != SQL_SUCCESS) {
			printf("Unable to prepare statement\n");
			exit(1);
		}
		if (SQLExecute(Statement) != SQL_SUCCESS) {
			printf("Unable to execute statement\n");
			exit(1);
		}
	} else {

		/* execute a batch command select insert insert select and check rows */
		if (SQLExecDirect(Statement, (SQLCHAR *) query, SQL_NTS) != SQL_SUCCESS) {
			printf("Unable to execute direct statement\n");
			exit(1);
		}
	}
	if (!prepare) {
		printf("Result %d\n", ++n);
		CheckCols(0);
		CheckRows(1);
		NextResults(SQL_SUCCESS);
	}
	printf("Result %d\n", ++n);
	CheckCols(1);
	CheckRows(-1);
	Fetch(SQL_SUCCESS);
	Fetch(SQL_SUCCESS);
	CheckCols(1);
	CheckRows(-1);
	Fetch(SQL_NO_DATA);
	CheckCols(1);
	CheckRows(2);
	NextResults(SQL_SUCCESS);
	if (!prepare) {
		printf("Result %d\n", ++n);
		CheckCols(0);
		CheckRows(1);
		NextResults(SQL_SUCCESS);
		printf("Result %d\n", ++n);
		CheckCols(0);
		CheckRows(2);
		NextResults(SQL_SUCCESS);
	}
	printf("Result %d\n", ++n);
	CheckCols(1);
	CheckRows(-1);
	Fetch(SQL_SUCCESS);
	CheckCols(1);
	CheckRows(-1);
	Fetch(SQL_NO_DATA);
	CheckCols(1);
	if (prepare) {
		CheckRows(2);
	} else {
		CheckRows(1);
		NextResults(SQL_SUCCESS);
		CheckCols(0);
		CheckRows(2);
	}

	NextResults(SQL_NO_DATA);
	if (!prepare)
		CheckCols(-1);
	CheckRows(-2);
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
