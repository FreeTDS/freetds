#include "common.h"

/* Test for data format returned from SQLPrepare */

static char software_version[] = "$Id: prepare_results.c,v 1.4 2004-10-28 13:16:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLSMALLINT count, namelen, type, digits, nullable;
	SQLULEN size;
	char name[128];

	Connect();

	Command(Statement, "create table #odbctestdata (i int, c char(20), n numeric(34,12) )");

	/* test query returns column information */
	if (SQLPrepare(Statement, "select * from #odbctestdata select * from #odbctestdata", SQL_NTS) != SQL_SUCCESS) {
		printf("SQLPrepare return failure\n");
		exit(1);
	}

	if (SQLNumResultCols(Statement, &count) != SQL_SUCCESS) {
		printf("SQLNumResultCols return failure\n");
		exit(1);
	}

	if (count != 3) {
		printf("Wrong number of columns returned. Got %d expected 2\n", (int) count);
		exit(1);
	}

	if (SQLDescribeCol(Statement, 1, name, sizeof(name), &namelen, &type, &size, &digits, &nullable) != SQL_SUCCESS) {
		printf("SQLDescribeCol failure for column 1\n");
		exit(1);
	}

	if (type != SQL_INTEGER || strcmp(name, "i") != 0) {
		printf("wrong column 1 informations\n");
		exit(1);
	}

	if (SQLDescribeCol(Statement, 2, name, sizeof(name), &namelen, &type, &size, &digits, &nullable) != SQL_SUCCESS) {
		printf("SQLDescribeCol failure for column 2\n");
		exit(1);
	}

	if (type != SQL_CHAR || strcmp(name, "c") != 0 || size != 20) {
		printf("wrong column 2 informations\n");
		exit(1);
	}

	if (SQLDescribeCol(Statement, 3, name, sizeof(name), &namelen, &type, &size, &digits, &nullable) != SQL_SUCCESS) {
		printf("SQLDescribeCol failure for column 3\n");
		exit(1);
	}

	if (type != SQL_NUMERIC || strcmp(name, "n") != 0 || size != 34 || digits != 12) {
		printf("wrong column 3 informations\n");
		exit(1);
	}

	/* TODO test SQLDescribeParam (when implemented) */
	Command(Statement, "drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
