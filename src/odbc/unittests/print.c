#include "common.h"

static char software_version[] = "$Id: print.c,v 1.19 2008-01-29 14:30:48 freddy77 Exp $";
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
	if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: %s\n", output);
}

static int
test(int odbc3)
{
	SQLLEN cnamesize;
	const char *query;
	SQLRETURN rc;

	use_odbc_version3 = odbc3;

	Connect();

	/* issue print statement and test message returned */
	output[0] = 0;
	query = "print 'START' select count(*) from sysobjects where name='sysobjects' print 'END'";
	if (CommandWithResult(Statement, query) != SQL_SUCCESS_WITH_INFO) {
		printf("SQLExecDirect should return SQL_SUCCESS_WITH_INFO\n");
		return 1;
	}
	ReadError();
	if (!strstr((char *) output, "START")) {
		printf("Message invalid\n");
		return 1;
	}
	output[0] = 0;

	if (odbc3) {
		CHECK_COLS(0);
		CHECK_ROWS(-1);
		rc = SQLFetch(Statement);
		if (rc != SQL_ERROR)
			ODBC_REPORT_ERROR("Still data?");
		CHK(SQLMoreResults, (Statement));
	}
    
	CHECK_COLS(1);
	CHECK_ROWS(-1);

	CHK(SQLFetch, (Statement));
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data?");
	CHECK_COLS(1);
	CHECK_ROWS(1);

	/* SQLMoreResults return NO DATA ... */
	rc = SQLMoreResults(Statement);
	if (rc != SQL_NO_DATA && rc != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("SQLMoreResults should return NO DATA or SUCCESS WITH INFO");

	if (tds_no_dm && !odbc3 && rc != SQL_NO_DATA)
		ODBC_REPORT_ERROR("SQLMoreResults should return NO DATA");

	if (odbc3 && rc != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("SQLMoreResults should return SUCCESS WITH INFO");

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

		rc = SQLMoreResults(Statement);
		if (rc != SQL_NO_DATA)
			ODBC_REPORT_ERROR("SQLMoreResults should return NO DATA");
	}

	/* issue invalid command and test error */
	if (CommandWithResult(Statement, "SELECT donotexistsfield FROM donotexiststable") != SQL_ERROR) {
		printf("SQLExecDirect returned strange results\n");
		return 1;
	}
	ReadError();

	/* test no data returned */
	if (SQLFetch(Statement) != SQL_ERROR) {
		printf("Row fetched ??\n");
		return 1;
	}
	ReadError();

	if (SQLGetData(Statement, 1, SQL_C_CHAR, output, sizeof(output), &cnamesize) != SQL_ERROR) {
		printf("Data ??\n");
		return 1;
	}
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

