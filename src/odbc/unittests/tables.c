#include "common.h"

static char software_version[] = "$Id: tables.c,v 1.4 2003-11-06 17:26:32 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLINTEGER cnamesize;
static SQLCHAR output[256];

static void
ReadCol(int i)
{
	if (SQLGetData(Statement, i, SQL_C_CHAR, output, sizeof(output), &cnamesize) != SQL_SUCCESS) {
		printf("Unable to get data col %d\n", i);
		CheckReturn();
		exit(1);
	}
}

static void
DoTest(const char *type, int row_returned)
{
	int len = 0;
	const char *p = NULL;

	if (*type) {
		len = strlen(type);
		p = type;
	}

	printf("Test type '%s' %s row\n", type, row_returned ? "with" : "without");
	if (!SQL_SUCCEEDED(SQLTables(Statement, NULL, 0, NULL, 0, "syscommentsgarbage", 11, (char *) p, len))) {
		printf("Unable to execute statement\n");
		CheckReturn();
		exit(1);
	}

	if (row_returned) {
		if (!SQL_SUCCEEDED(SQLFetch(Statement))) {
			printf("Unable to fetch row\n");
			CheckReturn();
			exit(1);
		}

		ReadCol(1);
		ReadCol(2);

		ReadCol(3);
		if (strcasecmp(output, "syscomments") != 0) {
			printf("wrong table %s\n", output);
			exit(1);
		}

		ReadCol(4);
		if (strcmp(output, "SYSTEM TABLE") != 0) {
			printf("wrong table type %s\n", output);
			exit(1);
		}

		ReadCol(5);

	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Unexpected data\n");
		CheckReturn();
		exit(1);
	}

	if (!SQL_SUCCEEDED(SQLCloseCursor(Statement))) {
		printf("Unable to close cursr\n");
		CheckReturn();
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	Connect();

	DoTest("", 1);
	DoTest("'SYSTEM TABLE'", 1);
	DoTest("'TABLE'", 0);
	DoTest("SYSTEM TABLE", 1);
	DoTest("TABLE", 0);
	DoTest("TABLE,VIEW", 0);
	DoTest("SYSTEM TABLE,'TABLE'", 1);
	DoTest("TABLE,'SYSTEM TABLE'", 1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
