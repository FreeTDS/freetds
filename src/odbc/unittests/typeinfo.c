#include "common.h"

static char software_version[] = "$Id: typeinfo.c,v 1.3 2004-10-13 18:06:57 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

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
FlushStatement(void)
{
	SQLRETURN retcode;

	while ((retcode = SQLFetch(Statement)) == SQL_SUCCESS);
	if (retcode != SQL_NO_DATA)
		ODBC_REPORT_ERROR("SQLFetch failed");

	/* Sybase store procedure seems to return extra empty results */
	while ((retcode = SQLMoreResults(Statement)) == SQL_SUCCESS);
	if (retcode != SQL_NO_DATA)
		ODBC_REPORT_ERROR("SQLMoreResults failed");
}

static void
CheckType(SQLSMALLINT type, SQLSMALLINT expected, const char *string_type)
{
	SQLSMALLINT out_type;
	SQLINTEGER ind;

	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 2, SQL_C_SSHORT, &out_type, 0, &ind)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLGetTypeInfo(Statement, type)))
		ODBC_REPORT_ERROR("SQLGetTypeInfo failed");
	switch (SQLFetch(Statement)) {
	case SQL_SUCCESS:
		if (expected == SQL_UNKNOWN_TYPE) {
			fprintf(stderr, "Data not expected (type %d - %s)\n", type, string_type);
			exit(1);
		}
		if (expected != out_type) {
			fprintf(stderr, "Got type %d expected %d. Input type %d - %s\n", out_type, expected, type, string_type);
			exit(1);
		}
		break;
	case SQL_NO_DATA:
		if (expected != SQL_UNKNOWN_TYPE) {
			fprintf(stderr, "Data expected. Inpute type %d - %s\n", type, string_type);
			exit(1);
		}
		break;
	default:
		ODBC_REPORT_ERROR("SQLFetch failed");
	}

	SQLFreeStmt(Statement, SQL_UNBIND);

	FlushStatement();
}

static void
DoTest(int version3)
{
	char name[128], params[128];
	SQLSMALLINT type, is_unsigned;
	SQLINTEGER col_size, min_scale, ind1, ind2, ind3, ind4, ind5, ind6;
	SQLRETURN retcode;

	use_odbc_version3 = version3;
	Connect();

	printf("Using ODBC version %d\n", version3 ? 3 : 2);

	/* test column name */
	if (!SQL_SUCCEEDED(SQLGetTypeInfo(Statement, SQL_ALL_TYPES)))
		ODBC_REPORT_ERROR("SQLGetTypeInfo failed");
	TestName(1, "TYPE_NAME");
	TestName(2, "DATA_TYPE");
	TestName(3, version3 ? "COLUMN_SIZE" : "PRECISION");
	TestName(4, "LITERAL_PREFIX");
	TestName(5, "LITERAL_SUFFIX");
	TestName(6, "CREATE_PARAMS");
	TestName(7, "NULLABLE");
	TestName(8, "CASE_SENSITIVE");
	TestName(9, "SEARCHABLE");
	TestName(10, "UNSIGNED_ATTRIBUTE");
	TestName(11, version3 ? "FIXED_PREC_SCALE" : "MONEY");
	TestName(12, version3 ? "AUTO_UNIQUE_VALUE" : "AUTO_INCREMENT");
	TestName(13, "LOCAL_TYPE_NAME");
	TestName(14, "MINIMUM_SCALE");
	TestName(15, "MAXIMUM_SCALE");

	/* TODO test these column for ODBC 3 */
	/* ODBC 3.0 SQL_DATA_TYPE SQL_DATETIME_SUB NUM_PREC_RADIX INTERVAL_PRECISION */

	FlushStatement();

	/* TODO test if SQL_ALL_TYPES returns right numeric type for timestamp */

	/* numeric type for data */
#define CHECK_TYPE(in,out) CheckType(in, out, #in)
	CHECK_TYPE(SQL_DATE, SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TIME, SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TYPE_DATE, SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TYPE_TIME, SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TIMESTAMP, version3 ? SQL_UNKNOWN_TYPE : SQL_TIMESTAMP);
	/* I hope to fix this ASAP -- freddy77 */
#ifdef ENABLE_DEVELOPING
	CHECK_TYPE(SQL_TYPE_TIMESTAMP, version3 ? SQL_TYPE_TIMESTAMP : SQL_UNKNOWN_TYPE);
#endif

	/* TODO implement this part of test */
	/* varchar/nvarchar before sysname */

	/* test binding (not all column, required for Oracle) */
	if (!SQL_SUCCEEDED(SQLGetTypeInfo(Statement, SQL_ALL_TYPES)))
		ODBC_REPORT_ERROR("SQLGetTypeInfo failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 1, SQL_C_CHAR, name, sizeof(name), &ind1)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 2, SQL_C_SSHORT, &type, 0, &ind2)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 3, SQL_C_SLONG, &col_size, 0, &ind3)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 6, SQL_C_CHAR, params, sizeof(params), &ind4)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 10, SQL_C_SSHORT, &is_unsigned, 0, &ind5)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	if (!SQL_SUCCEEDED(SQLBindCol(Statement, 14, SQL_C_SSHORT, &min_scale, 0, &ind6)))
		ODBC_REPORT_ERROR("SQLBindCol failed");
	while ((retcode = SQLFetch(Statement)) == SQL_SUCCESS);
	if (retcode != SQL_NO_DATA)
		ODBC_REPORT_ERROR("SQLFetch failed");
	SQLFreeStmt(Statement, SQL_UNBIND);
	FlushStatement();

	Disconnect();
}

int
main(int argc, char *argv[])
{
	DoTest(0);

	DoTest(1);

	printf("Done.\n");
	return 0;
}
