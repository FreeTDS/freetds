#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0003.c,v 1.12 2003-03-27 10:01:09 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char command[512];

	Connect();

	sprintf(command, "drop table #odbctestdata");
	printf("%s\n", command);
	if (SQLExecDirect(Statement, (SQLCHAR *) command, SQL_NTS)
	    != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
	}

	Command(Statement, "create table #odbctestdata (i int)");

	/* test that 2 empty result set are returned correctly */
	Command(Statement, "select * from #odbctestdata select * from #odbctestdata");

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_SUCCESS) {
		printf("Expected another recordset\n");
		exit(1);
	}
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	/* test that skipping a no empty result go to other result set */
	Command(Statement, "insert into #odbctestdata values(123)");
	Command(Statement, "select * from #odbctestdata select * from #odbctestdata");

	if (SQLMoreResults(Statement) != SQL_SUCCESS) {
		printf("Expected another recordset\n");
		exit(1);
	}
	printf("Getting next recordset\n");

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Expecting a row\n");
		exit(1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	Command(Statement, "drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
