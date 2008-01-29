#include "common.h"

/* Test for executing SQLExecute and rebinding parameters */

static char software_version[] = "$Id: rebindpar.c,v 1.7 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
TestInsert(HSTMT stmt, char *buf)
{
	SQLLEN ind;
	int l = strlen(buf);
	char sql[200];

	/* insert some data and test success */
	CHK(SQLBindParameter, (stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, l, 0, buf, l, &ind));

	ind = l;
	CHK(SQLExecute, (stmt));

	sprintf(sql, "SELECT 1 FROM #tmp1 WHERE c = '%s'", buf);
	Command(Statement, sql);
	CHK(SQLFetch, (Statement));
	if (SQLFetch(Statement) != SQL_NO_DATA || SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "One row expected!\n");
		exit(1);
	}
}

static void
Test(int prebind)
{
	SQLLEN ind;
	int i;
	char buf[100];
	HSTMT stmt;

	/* build a string longer than 80 character (80 it's the default) */
	buf[0] = 0;
	for (i = 0; i < 21; ++i)
		strcat(buf, "miao");

	Command(Statement, "DELETE FROM #tmp1");

	CHK(SQLAllocStmt, (Connection, &stmt));

	if (prebind)
		CHK(SQLBindParameter, (stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 1, 0, buf, 1, &ind));

	CHK(SQLPrepare, (stmt, (SQLCHAR *) "INSERT INTO #tmp1(c) VALUES(?)", SQL_NTS));

	/* try to insert al empty string, should not fail */
	/* NOTE this is currently the only test for insert a empty string using rpc */
	if (db_is_microsoft())
		TestInsert(stmt, "");
	TestInsert(stmt, "a");
	TestInsert(stmt, "bb");
	TestInsert(stmt, buf);

	CHK(SQLFreeStmt, (stmt, SQL_DROP));
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
