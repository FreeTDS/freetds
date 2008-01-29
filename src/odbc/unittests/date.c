#include "common.h"


static char software_version[] = "$Id: date.c,v 1.10 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
DoTest(int n)
{
	int res;

	SQLCHAR output[256];

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLLEN dataSize;

	TIMESTAMP_STRUCT ts;

	Command(Statement, "select convert(datetime, '2002-12-27 18:43:21')");

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to fetch row");

	CHK(SQLDescribeCol, (Statement, 1, output, sizeof(output), NULL, &colType, &colSize, &colScale, &colNullable));

	if (n == 0) {
		memset(&ts, 0, sizeof(ts));
		CHK(SQLGetData, (Statement, 1, SQL_C_TIMESTAMP, &ts, sizeof(ts), &dataSize));
		sprintf((char *) output, "%04d-%02d-%02d %02d:%02d:%02d.000", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
	} else {
		CHK(SQLGetData, (Statement, 1, SQL_C_CHAR, output, sizeof(output), &dataSize));
	}

	printf("Date returned: %s\n", output);
	if (strcmp((char *) output, "2002-12-27 18:43:21.000") != 0) {
		fprintf(stderr, "Invalid returned date\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Unable to fetch row");

	res = SQLCloseCursor(Statement);
	if (!SQL_SUCCEEDED(res))
		ODBC_REPORT_ERROR("Unable to close cursor");
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
