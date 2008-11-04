#include "common.h"

static char software_version[] = "$Id: typeinfo.c,v 1.10 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
TestName(int index, const char *expected_name)
{
	char name[128];
	char buf[256];
	SQLSMALLINT len, type;

#define NAME_TEST \
	do { \
		if (strcmp(name, expected_name) != 0) \
		{ \
			sprintf(buf, "wrong name in column %d expected '%s' got '%s'", index, expected_name, name); \
			ODBC_REPORT_ERROR(buf); \
		} \
	} while(0)

	/* retrieve with SQLDescribeCol */
	CHKDescribeCol(index, (SQLCHAR *) name, sizeof(name), &len, &type, NULL, NULL, NULL, "S");
	NAME_TEST;

	/* retrieve with SQLColAttribute */
	CHKColAttribute(index, SQL_DESC_NAME, name, sizeof(name), &len, NULL, "S");
	if (db_is_microsoft())
		NAME_TEST;
	CHKColAttribute(index, SQL_DESC_LABEL, name, sizeof(name), &len, NULL, "S");
	NAME_TEST;
}

static void
FlushStatement(void)
{
	SQLRETURN RetCode;

	while (CHKFetch("SNo") == SQL_SUCCESS)
		;

	/* Sybase store procedure seems to return extra empty results */
	while (CHKMoreResults("SNo") == SQL_SUCCESS)
		;
}

static void
CheckType(SQLSMALLINT type, SQLSMALLINT expected, const char *string_type, int line)
{
	SQLSMALLINT out_type;
	SQLLEN ind;
	SQLRETURN RetCode;

	CHKBindCol(2, SQL_C_SSHORT, &out_type, 0, &ind, "SI");
	CHKGetTypeInfo(type, "SI");
	CHKFetch("SNo");
	switch (RetCode) {
	case SQL_SUCCESS:
		if (expected == SQL_UNKNOWN_TYPE) {
			fprintf(stderr, "Data not expected (type %d - %s) line %d\n", type, string_type, line);
			Disconnect();
			exit(1);
		}
		if (expected != out_type) {
			fprintf(stderr, "Got type %d expected %d. Input type %d - %s line %d\n", out_type, expected, type, string_type, line);
			Disconnect();
			exit(1);
		}
		break;
	case SQL_NO_DATA:
		if (expected != SQL_UNKNOWN_TYPE) {
			fprintf(stderr, "Data expected. Inpute type %d - %s line %d\n", type, string_type, line);
			Disconnect();
			exit(1);
		}
		break;
	}

	SQLFreeStmt(Statement, SQL_UNBIND);

	FlushStatement();
}

static void
DoTest(int version3)
{
	char name[128], params[128];
	SQLSMALLINT type, is_unsigned;
	SQLINTEGER col_size, min_scale;
	SQLLEN ind1, ind2, ind3, ind4, ind5, ind6;
	SQLRETURN retcode;
	int date_time_supported = 0;

	use_odbc_version3 = version3;
	Connect();

	printf("Using ODBC version %d\n", version3 ? 3 : 2);

	/* test column name */
	CHKGetTypeInfo(SQL_ALL_TYPES, "SI");
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

	/* test for date/time support */
	retcode = CommandWithResult(Statement, "select cast(getdate() as date)");
	if (retcode == SQL_SUCCESS)
		date_time_supported = 1;
	SQLCloseCursor(Statement);

#define CHECK_TYPE(in,out) CheckType(in, out, #in, __LINE__)

	/* under Sybase this type require extra handling, check it */
	CHECK_TYPE(SQL_VARCHAR, SQL_VARCHAR);

	CHECK_TYPE(SQL_DATE, date_time_supported && !version3 ? SQL_DATE : SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TIME, date_time_supported && !version3 ? SQL_TIME : SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TYPE_DATE, date_time_supported && version3 ? SQL_TYPE_DATE : SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TYPE_TIME, date_time_supported && version3 ? SQL_TYPE_TIME : SQL_UNKNOWN_TYPE);
	CHECK_TYPE(SQL_TIMESTAMP, version3 ? SQL_UNKNOWN_TYPE : SQL_TIMESTAMP);
	CHECK_TYPE(SQL_TYPE_TIMESTAMP, version3 ? SQL_TYPE_TIMESTAMP : SQL_UNKNOWN_TYPE);

	/* TODO implement this part of test */
	/* varchar/nvarchar before sysname */

	/* test binding (not all column, required for Oracle) */
	CHKGetTypeInfo(SQL_ALL_TYPES, "SI");
	CHKBindCol(1, SQL_C_CHAR, name, sizeof(name), &ind1, "SI");
	CHKBindCol(2, SQL_C_SSHORT, &type, 0, &ind2, "SI");
	CHKBindCol(3, SQL_C_SLONG, &col_size, 0, &ind3, "SI");
	CHKBindCol(6, SQL_C_CHAR, params, sizeof(params), &ind4, "SI");
	CHKBindCol(10, SQL_C_SSHORT, &is_unsigned, 0, &ind5, "SI");
	CHKBindCol(14, SQL_C_SSHORT, &min_scale, 0, &ind6, "SI");
	while (CHKFetch("SNo") == SQL_SUCCESS)
		;
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
