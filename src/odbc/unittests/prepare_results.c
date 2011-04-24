#include "common.h"

/* Test for data format returned from SQLPrepare */

static char software_version[] = "$Id: prepare_results.c,v 1.14 2011-04-24 18:24:34 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(int use_ird)
{
	SQLSMALLINT count, namelen, type, digits, nullable;
	SQLULEN size;
	SQLHDESC desc;
	SQLINTEGER ind;
	char name[128];

	/* test query returns column information */
	CHKPrepare((SQLCHAR *) "select * from #odbctestdata select * from #odbctestdata", SQL_NTS, "S");

	SQLNumParams(odbc_stmt, &count);
	if (count != 0) {
		fprintf(stderr, "Wrong number of params returned. Got %d expected 0\n", (int) count);
		exit(1);
	}

	if (use_ird) {
		/* get IRD */
		CHKGetStmtAttr(SQL_ATTR_IMP_ROW_DESC, &desc, sizeof(desc), &ind, "S");

		CHKR(SQLGetDescField, (desc, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind), "S");
	} else {
		CHKNumResultCols(&count, "S");
	}

	if (count != 3) {
		fprintf(stderr, "Wrong number of columns returned. Got %d expected 3\n", (int) count);
		exit(1);
	}

	CHKDescribeCol(1, (SQLCHAR *) name, sizeof(name), &namelen, &type, &size, &digits, &nullable, "S");

	if (type != SQL_INTEGER || strcmp(name, "i") != 0) {
		fprintf(stderr, "wrong column 1 informations (type %d name '%s' size %d)\n", (int) type, name, (int) size);
		exit(1);
	}

	CHKDescribeCol(2, (SQLCHAR *) name, sizeof(name), &namelen, &type, &size, &digits, &nullable, "S");

	if (type != SQL_CHAR || strcmp(name, "c") != 0 || (size != 20 && (odbc_db_is_microsoft() || size != 40))) {
		fprintf(stderr, "wrong column 2 informations (type %d name '%s' size %d)\n", (int) type, name, (int) size);
		exit(1);
	}

	CHKDescribeCol(3, (SQLCHAR *) name, sizeof(name), &namelen, &type, &size, &digits, &nullable, "S");

	if (type != SQL_NUMERIC || strcmp(name, "n") != 0 || size != 34 || digits != 12) {
		fprintf(stderr, "wrong column 3 informations (type %d name '%s' size %d)\n", (int) type, name, (int) size);
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	SQLSMALLINT count;

	odbc_connect();

	odbc_command("create table #odbctestdata (i int, c char(20), n numeric(34,12) )");

	/* reset state */
	odbc_command("select * from #odbctestdata");
	SQLFetch(odbc_stmt);
	SQLMoreResults(odbc_stmt);

	/* test query returns column information for select without param */
	odbc_reset_statement();
	CHKPrepare((SQLCHAR *) "select * from #odbctestdata", SQL_NTS, "S");

	count = -1;
	CHKNumResultCols(&count, "S");

	if (count != 3) {
		fprintf(stderr, "Wrong number of columns returned. Got %d expected 3\n", (int) count);
		exit(1);
	}

	/* test query returns column information for select with param */
	odbc_reset_statement();
	CHKPrepare((SQLCHAR *) "select c,n from #odbctestdata where i=?", SQL_NTS, "S");

	count = -1;
	CHKNumResultCols(&count, "S");

	if (count != 2) {
		fprintf(stderr, "Wrong number of columns returned. Got %d expected 2\n", (int) count);
		exit(1);
	}

	/* test query returns column information for update */
	odbc_reset_statement();
	CHKPrepare((SQLCHAR *) "update #odbctestdata set i = 20", SQL_NTS, "S");

	count = -1;
	CHKNumResultCols(&count, "S");

	if (count != 0) {
		fprintf(stderr, "Wrong number of columns returned. Got %d expected 0\n", (int) count);
		exit(1);
	}

	Test(0);
	odbc_reset_statement();
	Test(1);

	/* TODO test SQLDescribeParam (when implemented) */
	odbc_command("drop table #odbctestdata");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
