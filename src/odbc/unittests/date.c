#include "common.h"
#include <odbcss.h>

static int
DoTest(int n)
{
	SQLCHAR output[256];

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLLEN dataSize;
	char const* expect;

	TIMESTAMP_STRUCT ts;

	switch (n)
	{
	case 0:
	case 1:
		odbc_command("select convert(datetime, '2002-12-27 18:43:21')");
		expect = "2002-12-27 18:43:21.000";
		break;

	case 2:
		/* Recent feature added to trunk - Full precision for datetime2 */
		odbc_command("select convert(datetime2, '2002-12-27 18:43:21')");
		expect = "2002-12-27 18:43:21.0000000";
		break;

	case 3:
		SQLSetConnectAttr(odbc_conn, SQL_COPT_TDSODBC_DATETIME_FORMAT, T("**%d-%m-%Y %H:%M:%S**"), SQL_NTS);
		odbc_command("select convert(datetime, '2002-12-27 18:43:21')");
		expect = "**27-12-2002 18:43:21**";
		break;

	case 4:
		/* MS datetime2 wire format introduced in TDS 7.3
		 * (earlier TDS versions use NVARCHAR wire format here) */
		if (odbc_tds_version() < 0x703)
			return 0;
		SQLSetConnectAttr(odbc_conn, SQL_COPT_TDSODBC_DATETIME_FORMAT, T("**%d-%m-%Y %H:%M:%S.%z**"), SQL_NTS);
		odbc_command("select convert(datetime2(7), '2002-12-27 18:43:21')");
		expect = "**27-12-2002 18:43:21.0000000**";
		break;

	case 5:
		/* MS date-only and time-only wire formats introduced in TDS 7.3 */
		if (odbc_tds_version() < 0x703)
			return 0;
		SQLSetConnectAttr(odbc_conn, SQL_COPT_TDSODBC_DATE_FORMAT, T("**%d-%m-%Y**"), SQL_NTS);
		odbc_command("select convert(date, '2002-12-27 18:43:21')");
		expect = "**27-12-2002**";
		break;

	case 6:
		if (odbc_tds_version() < 0x703)
			return 0;
		SQLSetConnectAttr(odbc_conn, SQL_COPT_TDSODBC_TIME_FORMAT, T("**%H:%M:%S**"), SQL_NTS);
		odbc_command("select convert(time, '2002-12-27 18:43:21')");
		expect = "**18:43:21**";
		break;

	default:
		printf("Done.\n");
		return 0;
	}

	CHKFetch("SI");
	CHKDescribeCol(1, (SQLTCHAR*)output, sizeof(output)/sizeof(SQLWCHAR), NULL, &colType, &colSize, &colScale, &colNullable, "S");

	/* Case 0 tests binding SQL_C_TIMESTAMP to result; other cases are testing binding to char. */
	if (n == 0) {
		memset(&ts, 0, sizeof(ts));
		CHKGetData(1, SQL_C_TIMESTAMP, &ts, sizeof(ts), &dataSize, "S");
		sprintf((char *) output, "%04d-%02d-%02d %02d:%02d:%02d.000", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
	} else {
		CHKGetData(1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");
	}

	printf("Date returned: \"%s\"\n", output);
	if (strcmp((char *) output, expect) != 0) {
		fprintf(stderr, "Invalid returned date; expected \"%s\".\n", expect);
		return 1;
	}

	if ((int)colSize < dataSize)
	{
		fprintf(stderr, "Column described as size %d, but data length was %d\n", (int)colSize, (int)dataSize);
		return 1;
	}
	CHKFetch("No");
	CHKCloseCursor("SI");
	return -1;
}

TEST_MAIN()
{
	int i = 0;
	int exit_status;
	odbc_connect();
	while ((exit_status = DoTest(i++)) == -1) {}
	odbc_disconnect();

	return exit_status;
}
