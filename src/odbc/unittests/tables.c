#include "common.h"

static char software_version[] = "$Id: tables.c,v 1.10 2005-11-04 13:42:28 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#ifdef WIN32
#undef strcasecmp
#define strcasecmp stricmp
#endif

static SQLLEN cnamesize;
static char output[256];

static void
ReadCol(int i)
{
	strcpy(output, "NULL");
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

static const char *catalog = NULL;
static const char *schema = NULL;
static const char *table = "syscomments";
static const char *expect = NULL;
static int expect_col = 3;

static void
DoTest(const char *type, int row_returned)
{
	int table_len = SQL_NULL_DATA;
	SQLRETURN ret;
	char table_buf[80];
	int found = 0;

#define PARAM(x) (SQLCHAR *) (x), (x) ? strlen(x) : SQL_NULL_DATA

	if (table) {
		strcpy(table_buf, table);
		strcat(table_buf, "garbage");
		table_len = strlen(table);
	}

	printf("Test type '%s' %s row\n", type ? type : "", row_returned ? "with" : "without");
	if (!SQL_SUCCEEDED(SQLTables(Statement, PARAM(catalog), PARAM(schema), (SQLCHAR *) table_buf, table_len, PARAM(type)))) {
		printf("Unable to execute statement\n");
		CheckReturn();
		exit(1);
	}

	/* test column name (for DBD::ODBC) */
	TestName(1, use_odbc_version3 || !driver_is_freetds() ? "TABLE_CAT" : "TABLE_QUALIFIER");
	TestName(2, use_odbc_version3 || !driver_is_freetds() ? "TABLE_SCHEM" : "TABLE_OWNER");
	TestName(3, "TABLE_NAME");
	TestName(4, "TABLE_TYPE");
	TestName(5, "REMARKS");

	if (row_returned) {
		if (!SQL_SUCCEEDED(SQLFetch(Statement))) {
			printf("Unable to fetch row\n");
			CheckReturn();
			exit(1);
		}

		if (!expect) {
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
	}

	if (expect) {
		ReadCol(expect_col);
		if (strcmp(output, expect) == 0)
			found = 1;
	}
	ret = SQLFetch(Statement);
	while (ret == SQL_SUCCESS && row_returned > 1) {
		if (expect) {
			ReadCol(expect_col);
			if (strcmp(output, expect) == 0)
				found = 1;
		}
		if (row_returned < 2)
			break;
		ret = SQLFetch(Statement);
	}

	if (ret != SQL_NO_DATA) {
		printf("Unexpected data\n");
		CheckReturn();
		exit(1);
	}

	if (expect && !found) {
		printf("expected row not found\n");
		exit(1);
	}

	if (!SQL_SUCCEEDED(SQLCloseCursor(Statement))) {
		printf("Unable to close cursr\n");
		CheckReturn();
		exit(1);
	}
	expect = NULL;
	expect_col = 3;
}

int
main(int argc, char *argv[])
{
	use_odbc_version3 = 0;
	Connect();

	DoTest(NULL, 1);
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
	/* TODO this should work ever for Sybase */
	if (db_is_microsoft()) {
		catalog = "%";
		DoTest(NULL, 2);
	}

	/*
	 * tests for Jdbc compatiblity
	 */

	/* enum tables */
	catalog = NULL;
	schema = NULL;
	table = "%";
	expect = "syscomments";
	DoTest(NULL, 2);

	/* enum catalogs */
	catalog = "%";
	schema = "";
	table = "";
	expect = "master";
	expect_col = 1;
	DoTest(NULL, 2);

	/* enum schemas (owners) */
	catalog = "";
	schema = "%";
	table = "";
	expect = "dbo";
	expect_col = 2;
	DoTest(NULL, 2);

	Disconnect();

	printf("Done.\n");
	return 0;
}
