#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0003.c,v 1.16 2005-03-29 15:19:36 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int prepared)
{
	Command(Statement, "create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		if (SQLPrepare(Statement, (SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare return failure");
		if (SQLExecute(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLExecure return failure");
	}

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected");

	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expected another recordset");
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected");

	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Not expected another recordset");

	/* test that skipping a no empty result go to other result set */
	Command(Statement, "insert into #odbctestdata values(123)");
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		if (SQLPrepare(Statement, (SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLPrepare return failure");
		if (SQLExecute(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLExecure return failure");
	}

	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expected another recordset");
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Expecting a row");

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected");

	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Not expected another recordset");

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
