#include "common.h"

static char software_version[] = "$Id: getdata.c,v 1.4 2007-08-07 09:20:32 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[16];
	SQLINTEGER int_buf;
	SQLLEN len;
	int ms_db = 0;

	Connect();

	/* TODO test with VARCHAR too */
	Command(Statement, "SELECT CONVERT(TEXT,'Prova')");

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	/* these 2 tests test an old severe BUG in FreeTDS */
	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 3, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "Pr") != 0) {
		printf("Wrong data result 1\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 16, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "ova") != 0) {
		printf("Wrong data result 2 res = '%s'\n", buf);
		exit(1);
	}

	ResetStatement();

	/* test with varchar, not blob but variable */
	Command(Statement, "SELECT CONVERT(VARCHAR(100), 'Other test')");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to fetch row");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 7, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "Other ") != 0) {
		printf("Wrong data result 1\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 16, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "test") != 0) {
		printf("Wrong data result 2 res = '%s'\n", buf);
		exit(1);
	}

	ResetStatement();

	/* test with fixed length */
	Command(Statement, "SELECT CONVERT(INT, 12345)");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to fetch row");

	int_buf = 0xdeadbeef;
	if (SQLGetData(Statement, 1, SQL_C_SLONG, &int_buf, 0, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (int_buf != 12345) {
		printf("Wrong data result\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_SLONG, &int_buf, 0, NULL) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Unable to get data");
	if (int_buf != 12345) {
		printf("Wrong data result 2 res = %d\n", (int) int_buf);
		exit(1);
	}

	ResetStatement();

	ms_db = db_is_microsoft();

	Disconnect();

	if (ms_db) {
		use_odbc_version3 = 1;
		Connect();

		Command(Statement, "SELECT CONVERT(TEXT,'')");

		if (SQLFetch(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to fetch row");

		len = 1234;
		if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 1, &len) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");

		if (len != 0) {
			fprintf(stderr, "Wrong len returned, returned %ld\n", (long) len);
			return 1;
		}

		if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 1, NULL) != SQL_NO_DATA)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");

		Disconnect();
	}

	printf("Done.\n");
	return 0;
}
