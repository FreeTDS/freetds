#include "common.h"

/* Test for executing SQLExecute and rebinding parameters */

static char software_version[] = "$Id: rebindpar.c,v 1.2 2004-04-09 08:57:54 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
TestInsert(HSTMT stmt, char *buf)
{
	SQLINTEGER ind;
	int l = strlen(buf);
	char sql[200];

	/* insert some data and test success */
	if (SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, l, 0, buf, l, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind");

	ind = l;
	if (SQLExecute(stmt) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute");

	sprintf(sql, "SELECT 1 FROM #tmp1 WHERE c = '%s'", buf);
	Command(Statement, sql);
	if (SQLFetch(Statement) != SQL_SUCCESS || SQLFetch(Statement) != SQL_NO_DATA || SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "One row expected!\n");
		exit(1);
	}
}

static void
Test(int prebind)
{
	SQLINTEGER ind;
	int i;
	char buf[100];
	HSTMT stmt;

	/* build a string longer than 80 character (80 it's the default) */
	buf[0] = 0;
	for (i = 0; i < 21; ++i)
		strcat(buf, "miao");

	Command(Statement, "DELETE FROM #tmp1");

	if (SQLAllocStmt(Connection, &stmt) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to allocate statement");

	if (prebind)
		if (SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 1, 0, buf, 1, &ind) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to bind");

	if (SQLPrepare(stmt, (SQLCHAR *) "INSERT INTO #tmp1(c) VALUES(?)", SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement");

	TestInsert(stmt, "a");
	TestInsert(stmt, "bb");
	TestInsert(stmt, buf);

	if (SQLFreeStmt(stmt, SQL_DROP) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to free statement");
}

int
main(int argc, char *argv[])
{
	Connect();

	Command(Statement, "CREATE TABLE #tmp1 (c VARCHAR(200))");

	Test(1);
	Test(0);

	Disconnect();

	printf("Done.\n");
	return 0;
}
