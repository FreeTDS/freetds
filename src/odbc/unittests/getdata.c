#include "common.h"

static char software_version[] = "$Id: getdata.c,v 1.3 2007-07-13 16:57:16 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[16];
	SQLLEN len;

	Connect();

	/* TODO test with VARCHAR too */
	Command(Statement, "SELECT CONVERT(TEXT,'Prova')");

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	/* these 2 tests test an old severe BUG in FreeTDS */
	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO) {
		printf("Unable to get data 1\n");
		CheckReturn();
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO) {
		printf("Unable to get data 2\n");
		CheckReturn();
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 3, NULL) != SQL_SUCCESS_WITH_INFO) {
		printf("Unable to get data \n");
		CheckReturn();
		exit(1);
	}
	if (strcmp(buf, "Pr") != 0) {
		printf("Wrong data result 1\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 16, NULL) != SQL_SUCCESS) {
		printf("Unable to get data \n");
		CheckReturn();
		exit(1);
	}
	if (strcmp(buf, "ova") != 0) {
		printf("Wrong data result 2 res = '%s'\n", buf);
		exit(1);
	}

	Disconnect();

	if (db_is_microsoft()) {
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
