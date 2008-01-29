#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0003.c,v 1.17 2008-01-29 14:30:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int prepared)
{
	Command(Statement, "create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	if (!prepared) {
		Command(Statement, "select * from #odbctestdata select * from #odbctestdata");
	} else {
		CHK(SQLPrepare, (Statement, (SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS));
		CHK(SQLExecute, (Statement));
	}

	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Data not expected");

	CHK(SQLMoreResults, (Statement));
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
		CHK(SQLPrepare, (Statement, (SQLCHAR *)"select * from #odbctestdata select * from #odbctestdata", SQL_NTS));
		CHK(SQLExecute, (Statement));
	}

	CHK(SQLMoreResults, (Statement));
	printf("Getting next recordset\n");

	CHK(SQLFetch, (Statement));

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
