#include "common.h"


static char software_version[] = "$Id: date.c,v 1.11 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int n)
{
	SQLCHAR output[256];

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLLEN dataSize;

	TIMESTAMP_STRUCT ts;

	Command(Statement, "select convert(datetime, '2002-12-27 18:43:21')");

	CHKFetch("SI");
	CHKDescribeCol(1, output, sizeof(output), NULL, &colType, &colSize, &colScale, &colNullable, "S");

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

int
main(int argc, char *argv[])
{
	Connect();

	DoTest(0);
	DoTest(1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
