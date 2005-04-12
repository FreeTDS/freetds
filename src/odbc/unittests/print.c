#include "common.h"

static char software_version[] = "$Id: print.c,v 1.15 2005-04-12 07:19:10 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLCHAR output[256];
static void ReadError(void);

static void
ReadError(void)
{
	if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: %s\n", output);
}

int
main(int argc, char *argv[])
{
	SQLLEN cnamesize;
	const char *query;
	SQLRETURN rc;

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
	CHECK_COLS(1);
	CHECK_ROWS(-1);

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLFetch no succeeded");
	CHECK_COLS(1);
	CHECK_ROWS(-1);
	if (SQLFetch(Statement) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Still data?");
	CHECK_COLS(1);
	CHECK_ROWS(1);

	/* SQLMoreResults return NO DATA ... */
	rc = SQLMoreResults(Statement);
#ifndef TDS_NO_DM
	if (rc != SQL_NO_DATA && rc != SQL_SUCCESS_WITH_INFO)
#else
	if (rc != SQL_NO_DATA)
#endif
		ODBC_REPORT_ERROR("SQLMoreResults should return NO DATA");

	/*
	 * ... but read error
	 * (unixODBC till 2.2.11 do not read errors on NO DATA, skip test)
	 */
#ifdef TDS_NO_DM
	output[0] = 0;
	ReadError();
	if (!strstr((char *) output, "END")) {
		printf("Message invalid\n");
		return 1;
	}
	output[0] = 0;

	CHECK_COLS(-1);
	CHECK_ROWS(-2);
#endif

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

	printf("Done.\n");
	return 0;
}
