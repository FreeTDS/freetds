#include "common.h"

static char software_version[] = "$Id: tables.c,v 1.5 2004-03-28 18:34:17 freddy77 Exp $";
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
TestName(int index, const char *expected_name)
{
	char name[128];
	char buf[256];
	SQLSMALLINT len, type;
	SQLRETURN rc;

#define NAME_TEST \
	do { \
		if (rc != SQL_SUCCESS) \
			ODBC_REPORT_ERROR("SQLDescribeCol failed"); \
		if (strcmp(name, expected_name) != 0) \
		{ \
			sprintf(buf, "wrong name in column %d expected '%s' got '%s'", index, expected_name, name); \
			ODBC_REPORT_ERROR(buf); \
		} \
	} while(0)

	/* retrieve with SQLDescribeCol */
	rc = SQLDescribeCol(Statement, index, (SQLCHAR *) name, sizeof(name), &len, &type, NULL, NULL, NULL);
	NAME_TEST;

	/* retrieve with SQLColAttribute */
	rc = SQLColAttribute(Statement, index, SQL_DESC_NAME, name, sizeof(name), &len, NULL);
	NAME_TEST;
	rc = SQLColAttribute(Statement, index, SQL_DESC_LABEL, name, sizeof(name), &len, NULL);
	NAME_TEST;
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

	/* test column name (for DBD::ODBC) */
	TestName(1, use_odbc_version3 ? "TABLE_CAT" : "TABLE_QUALIFIER");
	TestName(2, use_odbc_version3 ? "TABLE_SCHEM" : "TABLE_OWNER");
	TestName(3, "TABLE_NAME");
	TestName(4, "TABLE_TYPE");
	TestName(5, "REMARKS");

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
	use_odbc_version3 = 0;
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

	use_odbc_version3 = 1;
	Connect();

	DoTest("'SYSTEM TABLE'", 1);

	Disconnect();

	printf("Done.\n");
	return 0;
}
