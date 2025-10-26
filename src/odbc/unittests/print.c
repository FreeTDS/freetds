#include "common.h"

static SQLCHAR output[256];

static int
test(bool odbc3)
{
	SQLLEN cnamesize;
	const char *query;

	odbc_use_version3 = odbc3;

	odbc_connect();

	/* issue print statement and test message returned */
	output[0] = 0;
	query = "print 'START' select count(*) from sysobjects where name='sysobjects' print 'END'";
	odbc_command2(query, "I");
	odbc_read_error();
	if (!strstr(odbc_err, "START")) {
		printf("Message invalid\n");
		return 1;
	}
	odbc_err[0] = 0;

	if (odbc3) {
		ODBC_CHECK_COLS(0);
		ODBC_CHECK_ROWS(-1);
		CHKFetch("E");
		CHKMoreResults("S");
	}
    
	ODBC_CHECK_COLS(1);
	ODBC_CHECK_ROWS(-1);

	CHKFetch("S");
	ODBC_CHECK_COLS(1);
	ODBC_CHECK_ROWS(-1);
	/* check no data */
	CHKFetch("No");
	ODBC_CHECK_COLS(1);
	ODBC_CHECK_ROWS(1);

	/* SQLMoreResults return NO DATA or SUCCESS WITH INFO ... */
	if (tds_no_dm && !odbc3)
		CHKMoreResults("No");
	else if (odbc3)
		CHKMoreResults("I");
	else
		CHKMoreResults("INo");

	/*
	 * ... but read error
	 * (unixODBC till 2.2.11 do not read errors on NO DATA, skip test)
	 */
	if (tds_no_dm || odbc3) {
		odbc_read_error();
		if (!strstr(odbc_err, "END")) {
			printf("Message invalid\n");
			return 1;
		}
		odbc_err[0] = 0;
	}

	if (odbc3) {
		ODBC_CHECK_COLS(0);
		ODBC_CHECK_ROWS(-1);

		CHKMoreResults("No");
	}

	/* issue invalid command and test error */
	odbc_command2("SELECT donotexistsfield FROM donotexiststable", "E");
	odbc_read_error();

	/* test no data returned */
	CHKFetch("E");
	odbc_read_error();

	CHKGetData(1, SQL_C_CHAR, output, sizeof(output), &cnamesize, "E");
	odbc_read_error();

	odbc_disconnect();

	return 0;
}

TEST_MAIN()
{
	int ret;

	/* ODBC 2 */
	ret = test(false);
	if (ret != 0)
		return ret;

	/* ODBC 3 */
	ret = test(true);
	if (ret != 0)
		return ret;

	printf("Done.\n");
	return 0;
}

