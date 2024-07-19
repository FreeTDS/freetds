#include "common.h"

static void
DoTest(int n)
{
	SQLCHAR output[256];

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLLEN dataSize;

	TIMESTAMP_STRUCT ts;

	odbc_command("select convert(datetime, '2002-12-27 18:43:21')");

	CHKFetch("SI");
	CHKDescribeCol(1, (SQLTCHAR*)output, sizeof(output)/sizeof(SQLWCHAR), NULL, &colType, &colSize, &colScale, &colNullable, "S");

	if (n == 0) {
		memset(&ts, 0, sizeof(ts));
		CHKGetData(1, SQL_C_TIMESTAMP, &ts, sizeof(ts), &dataSize, "S");
		sprintf((char *) output, "%04d-%02d-%02d %02d:%02d:%02d.000", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
	} else {
		CHKGetData(1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");
	}

	printf("Date returned: %s\n", output);
	if (strcmp((char *) output, "2002-12-27 18:43:21.000") != 0) {
		fprintf(stderr, "Invalid returned date\n");
		exit(1);
	}

	CHKFetch("No");
	CHKCloseCursor("SI");
}

TEST_MAIN()
{
	odbc_connect();

	DoTest(0);
	DoTest(1);

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
