#include "common.h"

/* Test for executing SQLExecute and rebinding parameters */

static char software_version[] = "$Id: rebindpar.c,v 1.1 2004-04-08 12:03:47 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static HSTMT stmt;

static void
TestInsert(char *buf)
{
	SQLINTEGER ind;
	int l = strlen(buf);
	char sql[200];

	/* insert some data and test success */
	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 1, 0, buf, l, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind");

	ind = l;
	if (SQLExecute(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute");

	sprintf(sql, "SELECT 1 FROM #tmp1 WHERE c = '%s'", buf);
	Command(stmt, sql);
	if (SQLFetch(stmt) != SQL_SUCCESS || SQLFetch(stmt) != SQL_NO_DATA || SQLMoreResults(stmt) != SQL_NO_DATA) {
		fprintf(stderr, "One row expected!\n");
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	char buf[100];
	int i;

	Connect();

	if (SQLAllocStmt(Connection, &stmt) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to allocate statement");

	Command(Statement, "CREATE TABLE #tmp1 (c VARCHAR(200))");

	if (SQLPrepare(Statement, (SQLCHAR *) "INSERT INTO #tmp1(c) VALUES(?)", SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement");

	TestInsert("a");
	TestInsert("bb");

	/* build a string longer than 80 character (80 it's the default) */
	buf[0] = 0;
	for (i = 0; i < 21; ++i)
		strcat(buf, "miao");
	TestInsert(buf);

	SQLFreeStmt(stmt, SQL_DROP);

	Disconnect();

	printf("Done.\n");
	return 0;
}
