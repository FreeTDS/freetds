/*
 * Verify buffered Data-At-Execution conversion of ODBC C structures
 * that require preprocessing before libTDS conversion.
 */

#include "common.h"

static bool
check_token(SQLPOINTER expected, SQLPOINTER actual)
{
	if (actual == expected)
		return true;

	fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", expected, actual);
	return false;
}

static bool
put_struct_parameter(SQLSMALLINT c_type, SQLSMALLINT sql_type, SQLULEN column_size, SQLSMALLINT scale,
	const void *value, SQLLEN value_size, SQLPOINTER token, SQLCHAR *query)
{
	SQLLEN ind;
	SQLLEN first_length;
	SQLPOINTER returned_token;

	ind = SQL_LEN_DATA_AT_EXEC(value_size);
	first_length = value_size / 2;

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(query, SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, c_type, sql_type, column_size, scale, token, value_size, &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (!check_token(token, returned_token)) {
		CHKFreeStmt(SQL_DROP, "S");
		return false;
	}

	/*
	 * Split the raw C representation. Neither chunk is independently
	 * a valid complete structure.
	 */
	CHKPutData((SQLPOINTER) value, first_length, "S");
	CHKPutData((SQLPOINTER) ((const char *) value + first_length), value_size - first_length, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");

	/*
	 * Each query returns one row only if preprocessing and final
	 * conversion produced the expected server value.
	 */
	CHKFetch("S");
	CHKFetch("No");
	CHKFreeStmt(SQL_DROP, "S");

	return true;
}

TEST_MAIN()
{
	SQL_NUMERIC_STRUCT numeric_value;
	DATE_STRUCT date_value;
	TIME_STRUCT time_value;
	TIMESTAMP_STRUCT timestamp_value;

	char numeric_token;
	char date_token;
	char time_token;
	char timestamp_token;

	odbc_connect();

	/*
	 * SQL_NUMERIC_STRUCT stores the unscaled integer in little-endian
	 * order. 123.45 is represented as the integer 12345 with scale 2.
	 */
	memset(&numeric_value, 0, sizeof(numeric_value));
	numeric_value.precision = 5;
	numeric_value.scale = 2;
	numeric_value.sign = 1;
	numeric_value.val[0] = 0x39;
	numeric_value.val[1] = 0x30;

	if (!put_struct_parameter(
			SQL_C_NUMERIC,
			SQL_NUMERIC,
			5,
			2,
			&numeric_value,
			sizeof(numeric_value),
			&numeric_token,
			T("SELECT 1 WHERE ? = CAST('123.45' AS NUMERIC(5,2))")
		))
		return 1;

	date_value.year = 2026;
	date_value.month = 11;
	date_value.day = 19;

	if (!put_struct_parameter(
			SQL_C_TYPE_DATE,
			SQL_TYPE_DATE,
			10,
			0,
			&date_value,
			sizeof(date_value),
			&date_token,
			T("SELECT 1 WHERE ? = CAST('2026-11-19' AS DATE)")
		))
		return 1;

	time_value.hour = 17;
	time_value.minute = 30;
	time_value.second = 45;

	if (!put_struct_parameter(
			SQL_C_TYPE_TIME,
			SQL_TYPE_TIME,
			8,
			0,
			&time_value,
			sizeof(time_value),
			&time_token,
			T("SELECT 1 WHERE ? = CAST('17:30:45' AS TIME(0))")
		))
		return 1;

	memset(&timestamp_value, 0, sizeof(timestamp_value));
	timestamp_value.year = 2026;
	timestamp_value.month = 11;
	timestamp_value.day = 19;
	timestamp_value.hour = 17;
	timestamp_value.minute = 30;
	timestamp_value.second = 45;

	if (!put_struct_parameter(
			SQL_C_TYPE_TIMESTAMP,
			SQL_TYPE_TIMESTAMP,
			19,
			0,
			&timestamp_value,
			sizeof(timestamp_value),
			&timestamp_token,
			T(
				"SELECT 1 WHERE ? = "
				"CAST('2026-11-19 17:30:45' AS DATETIME2(0))"
			)
		))
		return 1;

	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
