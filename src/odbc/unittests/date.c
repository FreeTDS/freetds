#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "common.h"


static char software_version[] = "$Id: date.c,v 1.1 2003-01-09 15:17:55 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{

	int res;

	SQLCHAR command[512];
	SQLCHAR output[256];

	SQLSMALLINT colType;
	SQLUINTEGER colSize;
	SQLSMALLINT colScale, colNullable;

	TIMESTAMP_STRUCT ts;

	Connect();

	sprintf(command, "select convert(datetime, '2002-12-27 18:43:21')");
	printf("%s\n", command);
	if (SQLExecDirect(Statement, command, SQL_NTS)
	    != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
		CheckReturn();
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	if (SQLDescribeCol(Statement, 1, (SQLCHAR *) output, sizeof(output), NULL, &colType, &colSize, &colScale, &colNullable) !=
	    SQL_SUCCESS) {
		printf("Error getting data\n");
		CheckReturn();
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_TIMESTAMP, &ts, sizeof(ts), &colSize) != SQL_SUCCESS) {
		printf("Unable to get data col %d\n", 1);
		CheckReturn();
		exit(1);
	}

	sprintf(output, "%04d-%02d-%02d", ts.year, ts.month, ts.day);
	printf("Date returned: %s\n", output);
	if (strcmp(output, "2002-12-27") != 0) {
		printf("Invalid returned date\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_NO_DATA) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	res = SQLCloseCursor(Statement);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to close cursr\n");
		CheckReturn();
		exit(1);
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
