#include "common.h"

/* Test for SQL_ATTR_ROW_NUMBER */

/*
 * This works under MS ODBC however I don't know if it's correct... 
 * TODO make it work and add to Makefile.am
 */

static char software_version[] = "$Id: rownumber.c,v 1.1 2004-07-30 14:22:08 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
CheckRows(int n, int line)
{
	SQLRETURN res;
	SQLUINTEGER value;

	res = SQLGetStmtAttr(Statement, SQL_ATTR_ROW_NUMBER, &value, sizeof(value), NULL);
	if (res != SQL_SUCCESS) {
		if (res == SQL_ERROR && n < 0)
			return;
		ODBC_REPORT_ERROR("SQLGetStmtAttr failed");
	}
	if (value != n) {
		fprintf(stderr, "Expected %d rows returned %d line %d\n", n, (int) value, line);
		exit(1);
	}
}

#define CHECK_ROWS(n) CheckRows(n,__LINE__)

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
DoTest()
{
	int n = 0;
	static const char query[] = "SELECT * FROM #tmp1 ORDER BY i SELECT * FROM #tmp1 WHERE i < 3 ORDER BY i";

	/* execute a batch command and check row number */
	if (SQLExecDirect(Statement, (SQLCHAR *) query, SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute direct statement");
	CHECK_ROWS(-1);
	printf("Result %d\n", ++n);
	Fetch(SQL_SUCCESS);
	CHECK_ROWS(0);
	Fetch(SQL_SUCCESS);
	CHECK_ROWS(0);
	Fetch(SQL_SUCCESS);
	CHECK_ROWS(0);
	Fetch(SQL_NO_DATA);
	CHECK_ROWS(-1);
	NextResults(SQL_SUCCESS);
	CHECK_ROWS(-1);

	printf("Result %d\n", ++n);
	Fetch(SQL_SUCCESS);
	CHECK_ROWS(0);
	Fetch(SQL_SUCCESS);
	CHECK_ROWS(0);
	Fetch(SQL_NO_DATA);
	CHECK_ROWS(-1);
	NextResults(SQL_NO_DATA);
	CHECK_ROWS(-1);
}

int
main(int argc, char *argv[])
{
	use_odbc_version3 = 1;

	Connect();

	Command(Statement, "create table #tmp1 (i int)");
	Command(Statement, "create table #tmp2 (i int)");
	Command(Statement, "insert into #tmp1 values(1)");
	Command(Statement, "insert into #tmp1 values(2)");
	Command(Statement, "insert into #tmp1 values(5)");

	DoTest();

	Disconnect();

	printf("Done.\n");
	return 0;
}
