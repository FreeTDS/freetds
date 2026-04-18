/* test SQLGetDescRec */
#include "common.h"

static void
check_int(bool cond, long int value, const char *msg, int line)
{
	if (cond)
		return;
	fprintf(stderr, "Invalid value %ld at line %d, check: %s\n", value, line, msg);
	exit(1);
}

#define check_int(value, cond, expected) \
	check_int(value cond expected, value, #value " " #cond " " #expected, __LINE__)

static void
test_column_number(void)
{
	SQLHDESC Descriptor;
	SQLINTEGER ind;
	SQLTCHAR name[128];
	SQLSMALLINT si;

	/* get IRD */
	CHKGetStmtAttr(SQL_ATTR_IMP_ROW_DESC, &Descriptor, sizeof(Descriptor), &ind, "S");

	/* Wrong column number */
	CHKGetDescRec(-1, name, TDS_VECTOR_SIZE(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "E");

	/* TODO here should be NO_DATA cause we are requesting bookmark */
	/* CHKGetDescRec(0, name, sizeof(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "No"); */

	/* No column present */
	CHKGetDescRec(1, name, TDS_VECTOR_SIZE(name), &si, NULL /*Type */ , NULL /*SubType */ , NULL /*Length */ ,
		      NULL /*Precision */ , NULL /*Scale */ , NULL /*Nullable */ , "No");

	odbc_command("SELECT name FROM #tmp1");

	/* Column present */
	CHKGetDescRec(1, name, TDS_VECTOR_SIZE(name), &si, NULL /*Type */ , NULL /*SubType */ , NULL /*Length */ ,
		      NULL /*Precision */ , NULL /*Scale */ , NULL /*Nullable */ , "S");
}

static void
test_ard_allocation(void)
{
	SQLHDESC ard;
	SQLINTEGER ind;
	SQLSMALLINT count;

	odbc_reset_statement();

	CHKGetStmtAttr(SQL_ATTR_APP_ROW_DESC, &ard, sizeof(ard), &ind, "S");

	/* A query should not extent ARD */
	odbc_command("SELECT name FROM #tmp1");
	CHKGetDescField(ard, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/* This should not extent ARD */
	CHKSetDescField(ard, 2, SQL_DESC_AUTO_UNIQUE_VALUE, TDS_INT2PTR(10), 0, "E");
	CHKGetDescField(ard, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/* This should extent ARD */
	CHKSetDescField(ard, 2, SQL_DESC_SCALE, TDS_INT2PTR(10), 0, "S");
	CHKGetDescField(ard, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 2);
}

TEST_MAIN()
{
	odbc_use_version3 = true;
	odbc_connect();

	odbc_command("create table #tmp1 (name varchar(100))");

	test_column_number();
	test_ard_allocation();

	odbc_disconnect();
	ODBC_FREE();
	return 0;
}
