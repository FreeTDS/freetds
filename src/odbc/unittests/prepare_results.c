#include "common.h"

/* Test for data format returned from SQLPrepare */

static char software_version[] = "$Id: prepare_results.c,v 1.11 2008-11-04 14:46:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLSMALLINT count, namelen, type, digits, nullable;
	SQLULEN size;
	char name[128];

	Connect();

	Command("create table #odbctestdata (i int, c char(20), n numeric(34,12) )");

	/* reset state */
	Command("select * from #odbctestdata");
	SQLFetch(Statement);
	SQLMoreResults(Statement);

	/* test query returns column information for update */
	CHKPrepare((SQLCHAR *) "update #odbctestdata set i = 20", SQL_NTS, "S");

	CHKNumResultCols(&count, "S");

	if (count != 0) {
		fprintf(stderr, "Wrong number of columns returned. Got %d expected 0\n", (int) count);
		exit(1);
	}

	/* test query returns column information */
	CHKPrepare((SQLCHAR *) "select * from #odbctestdata select * from #odbctestdata", SQL_NTS, "S");

	CHKNumResultCols(&count, "S");

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

	if (type != SQL_CHAR || strcmp(name, "c") != 0 || (size != 20 && (db_is_microsoft() || size != 40))) {
		fprintf(stderr, "wrong column 2 informations (type %d name '%s' size %d)\n", (int) type, name, (int) size);
		exit(1);
	}

	CHKDescribeCol(3, (SQLCHAR *) name, sizeof(name), &namelen, &type, &size, &digits, &nullable, "S");

	if (type != SQL_NUMERIC || strcmp(name, "n") != 0 || size != 34 || digits != 12) {
		fprintf(stderr, "wrong column 3 informations (type %d name '%s' size %d)\n", (int) type, name, (int) size);
		exit(1);
	}

	/* TODO test SQLDescribeParam (when implemented) */
	Command("drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
