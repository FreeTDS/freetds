#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0003.c,v 1.14 2003-06-03 09:48:09 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int prepared)
{
	Command(Statement, "create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		if (SQLPrepare(Statement, "select * from #odbctestdata select * from #odbctestdata", SQL_NTS) != SQL_SUCCESS) {
			printf("SQLPrepare return failure\n");
			exit(1);
		}
		if (SQLExecute(Statement) != SQL_SUCCESS) {
			printf("SQLExecure return failure\n");
			exit(1);
		}
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_SUCCESS) {
		printf("Expected another recordset\n");
		exit(1);
	}
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	/* test that skipping a no empty result go to other result set */
	Command(Statement, "insert into #odbctestdata values(123)");
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		if (SQLPrepare(Statement, "select * from #odbctestdata select * from #odbctestdata", SQL_NTS) != SQL_SUCCESS) {
			printf("SQLPrepare return failure\n");
			exit(1);
		}
		if (SQLExecute(Statement) != SQL_SUCCESS) {
			printf("SQLExecure return failure\n");
			exit(1);
		}
	}

	if (SQLMoreResults(Statement) != SQL_SUCCESS) {
		printf("Expected another recordset\n");
		exit(1);
	}
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Expecting a row\n");
		exit(1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

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
