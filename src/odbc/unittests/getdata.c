#include "common.h"

static char software_version[] = "$Id: getdata.c,v 1.1 2003-08-29 15:07:11 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[16];

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

	printf("Done.\n");
	return 0;
}
