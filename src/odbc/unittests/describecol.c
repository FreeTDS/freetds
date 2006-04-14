#include "common.h"

/*
 * SQLDescribeCol test for precision
 * test what say SQLDescribeCol about precision using some type
 */

static char software_version[] = "$Id: describecol.c,v 1.1 2006-04-14 11:55:31 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int g_result = 0;

static void
DoTest(const char *type, const char *val, int precision)
{
	SQLCHAR output[16];
	char sql[256];

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;

	sprintf(sql, "SELECT CAST(%s AS %s) AS col", val, type);
	/* ignore error, we only need precision of known types */
	if (CommandWithResult(Statement, sql) != SQL_SUCCESS) {
		ResetStatement();
		return;
	}

	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		ODBC_REPORT_ERROR("Unable to fetch row");

	colSize = 0xbeef;
	if (SQLDescribeCol(Statement, 1, output, sizeof(output), NULL, &colType, &colSize, &colScale, &colNullable) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Error getting data");

	if (colSize != precision) {
		g_result = 1;
		fprintf(stderr, "Expected precision %d got %d\n", precision, (int) colSize);
	}
	SQLMoreResults(Statement);
	ResetStatement();
}

int
main(int argc, char *argv[])
{
	Connect();

	DoTest("tinyint", "0", 3);
	DoTest("smallint", "0", 5);
	DoTest("int", "0", 10);

	DoTest("money", "0", 19);
	DoTest("smallmoney", "0", 10);

	DoTest("numeric(10,2)", "0", 10);
	DoTest("numeric(23,4)", "0", 23);

	DoTest("char(10)", "'hi!'", 10);
	DoTest("varchar(11)", "'hi!'", 11);
	DoTest("nchar(12)", "'hi!'", 12);
	DoTest("nvarchar(13)", "'hi!'", 13);

	Disconnect();

	printf("Done.\n");
	return g_result;
}

