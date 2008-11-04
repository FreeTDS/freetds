#include "common.h"

static char software_version[] = "$Id: print.c,v 1.21 2008-11-04 14:46:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLCHAR output[256];
static void ReadError(void);

#ifdef TDS_NO_DM
static const int tds_no_dm = 1;
#else
static const int tds_no_dm = 0;
#endif

static void
ReadError(void)
{
	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL, "SI");
	printf("Message: %s\n", output);
}

static int
test(int odbc3)
{
	SQLLEN cnamesize;
	const char *query;

	use_odbc_version3 = odbc3;

	Connect();

	/* issue print statement and test message returned */
	output[0] = 0;
	query = "print 'START' select count(*) from sysobjects where name='sysobjects' print 'END'";
	CHKR(CommandWithResult, (Statement, query), "I");
	ReadError();
	if (!strstr((char *) output, "START")) {
		printf("Message invalid\n");
		return 1;
	}
	output[0] = 0;

	if (odbc3) {
		CHECK_COLS(0);
		CHECK_ROWS(-1);
		CHKFetch("E");
		CHKMoreResults("S");
	}
    
	CHECK_COLS(1);
	CHECK_ROWS(-1);

	CHKFetch("S");
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	/* check no data */
	CHKFetch("No");
	CHECK_COLS(1);
	CHECK_ROWS(1);

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
		output[0] = 0;
		ReadError();
		if (!strstr((char *) output, "END")) {
			printf("Message invalid\n");
			return 1;
		}
		output[0] = 0;
	}

	if (!odbc3) {
		if (tds_no_dm) {
#if 0
			CHECK_COLS(-1);
#endif
			CHECK_ROWS(-2);
		}
	} else {
		CHECK_COLS(0);
		CHECK_ROWS(-1);

		CHKMoreResults("No");
	}

	/* issue invalid command and test error */
	CHKR(CommandWithResult, (Statement, "SELECT donotexistsfield FROM donotexiststable"), "E");
	ReadError();

	/* test no data returned */
	CHKFetch("E");
	ReadError();

	CHKGetData(1, SQL_C_CHAR, output, sizeof(output), &cnamesize, "E");
	ReadError();

	Disconnect();

	return 0;
}

int
main(int argc, char *argv[])
{
	int ret;

	/* ODBC 2 */
	ret = test(0);
	if (ret != 0)
		return ret;

	/* ODBC 3 */
	ret = test(1);
	if (ret != 0)
		return ret;

	printf("Done.\n");
	return 0;
}

