#include "common.h"
#include <assert.h>

/* Test compute results */

/*
 * This it's quite important cause it test different result types
 * mssql odbc have also some extension not supported by FreeTDS
 * and declared in odbcss.h
 */

static char software_version[] = "$Id: compute.c,v 1.2 2004-12-14 10:09:04 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char col1[256], col2[256];
static SQLLEN ind1, ind2;

static void
CheckFetch(const char *c1, const char *c2)
{
	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("error fetching");

	if (strlen(c1) != ind1 || strcmp(c1, col1) != 0) {
		fprintf(stderr, "Column 1 error '%s' (%d) expected '%s' (%d)\n", col1, (int) ind1, c1, strlen(c1));
		exit(1);
	}

	if (strlen(c2) != ind2 || strcmp(c2, col2) != 0) {
		fprintf(stderr, "Column 2 error '%s' (%d) expected '%s' (%d)\n", col2, (int) ind2, c2, strlen(c2));
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	Connect();

	Command(Statement, "create table #tmp1 (c varchar(20), i int)");
	Command(Statement, "insert into #tmp1 values('pippo', 12)");
	Command(Statement, "insert into #tmp1 values('pippo', 34)");
	Command(Statement, "insert into #tmp1 values('pluto', 1)");
	Command(Statement, "insert into #tmp1 values('pluto', 2)");
	Command(Statement, "insert into #tmp1 values('pluto', 3)");




	/* select * from #tmp1 compute sum(i) */
	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	Command(Statement, "select * from #tmp1 order by c, i compute sum(i)");
	CheckFetch("pippo", "12");
	CheckFetch("pippo", "34");
	CheckFetch("pluto", "1");
	CheckFetch("pluto", "2");
	CheckFetch("pluto", "3");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Still data ??");

	/* why I need to rebind ?? ms bug of feature ?? */
	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	col2[0] = '@';
	CheckFetch("52", "@");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");




	/* select * from #tmp1 order by c compute sum(i) by c */
	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	Command(Statement, "select * from #tmp1 order by c, i compute sum(i) by c");
	CheckFetch("pippo", "12");
	CheckFetch("pippo", "34");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Still data ??");

	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	strcpy(col2, "##");
	CheckFetch("46", "##");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Still data ??");

	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	CheckFetch("pluto", "1");
	CheckFetch("pluto", "2");
	CheckFetch("pluto", "3");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Still data ??");

	SQLBindCol(Statement, 1, SQL_C_CHAR, col1, sizeof(col1), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, col2, sizeof(col2), &ind2);
	strcpy(col2, "%");
	CheckFetch("6", "%");
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");
	if (SQLMoreResults(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data ??");




	Disconnect();
	return 0;
}
