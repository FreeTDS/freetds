/* Test SQLDescribeParam */

#include "common.h"

static void
check_int(bool cond, long int value, const char *msg, int line)
{
	if (cond)
		return;
	fprintf(stderr, "Invalid value %ld at line %d, check: %s\n", value, line, msg);
	exit(1);
}

#define check_int(value, cond, expected) \
	check_int(value cond expected, value, #value " " #cond " " #expected, __LINE__)

static void
check_type(bool cond, SQLSMALLINT value, const char *msg, int line)
{
	if (cond)
		return;
	fprintf(stderr, "Invalid value %d(%s) at line %d, check: %s\n",
		value, odbc_lookup_value(value, odbc_sql_types, "???"), line, msg);
	exit(1);
}

#define check_type(value, cond, expected) \
	check_type(value cond expected, value, #value " " #cond " " #expected, __LINE__)

TEST_MAIN()
{
	SQLSMALLINT num_params;
	SQLSMALLINT sql_type;
	SQLULEN size;
	SQLSMALLINT digits, scale, nullable, count;
	SQLHDESC ipd, apd;
	SQLINTEGER ind;
	SQLLEN sql_nts = SQL_NTS;
	SQLINTEGER id;
	const char *env;

	odbc_use_version3 = true;
	odbc_connect();

	if (!odbc_db_is_microsoft() || odbc_db_version_int() < 0x0b000000u) {
		odbc_disconnect();
		printf("SQLDescribeParam implementation requires MSSQL 2012+\n");
		odbc_test_skipped();
		return 0;
	}

	odbc_command("IF OBJECT_ID('describe') IS NOT NULL DROP TABLE describe");
	odbc_command("CREATE TABLE describe(i int NOT NULL, vc VARCHAR(100) NULL, "
		     "vb VARBINARY(100) NULL, num NUMERIC(17,5) NULL)");
	odbc_reset_statement();

	CHKPrepare(T("INSERT INTO describe(i, vc, num) VALUES(?, ?, ?)"), SQL_NTS, "S");

	/* we prepared a query with 3 parameters, we should have 3 parameters */
	CHKNumParams(&num_params, "S");
	check_int(num_params, ==, 3);

	CHKGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC, &ipd, sizeof(ipd), &ind, "S");
	CHKGetStmtAttr(SQL_ATTR_APP_PARAM_DESC, &apd, sizeof(apd), &ind, "S");

	/* check we have no parameters on IPD and APD */
	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/* get some parameters */
	CHKDescribeParam(0, &sql_type, &size, &digits, &nullable, "E");
	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_INTEGER);
	check_int(size, ==, 10);
	check_int(digits, ==, 0);
	check_int(nullable, ==, SQL_NULLABLE);
	CHKGetDescField(ipd, 1, SQL_DESC_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_INTEGER);
	CHKGetDescField(ipd, 1, SQL_DESC_CONCISE_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_INTEGER);
	CHKGetDescField(ipd, 1, SQL_DESC_LENGTH, &size, sizeof(SQLULEN), &ind, "S");
	check_int(size, ==, 10);
	CHKGetDescField(ipd, 1, SQL_DESC_PRECISION, &digits, sizeof(SQLSMALLINT), &ind, "S");
	/* TODO sligthly difference with MS driver about descriptor handling */
	if (!odbc_driver_is_freetds())
		check_int(digits, ==, 10);
	CHKGetDescField(ipd, 1, SQL_DESC_SCALE, &scale, sizeof(SQLSMALLINT), &ind, "S");
	check_int(scale, ==, 0);

	CHKDescribeParam(2, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_VARCHAR);
	check_int(size, ==, 100);
	check_int(digits, ==, 0);
	check_int(nullable, ==, SQL_NULLABLE);
	CHKGetDescField(ipd, 2, SQL_DESC_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_VARCHAR);
	CHKGetDescField(ipd, 2, SQL_DESC_CONCISE_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_VARCHAR);
	CHKGetDescField(ipd, 2, SQL_DESC_LENGTH, &size, sizeof(SQLULEN), &ind, "S");
	check_int(size, ==, 100);
	CHKGetDescField(ipd, 2, SQL_DESC_PRECISION, &digits, sizeof(SQLSMALLINT), &ind, "S");
	if (!odbc_driver_is_freetds())
		check_int(digits, ==, 100);
	CHKGetDescField(ipd, 2, SQL_DESC_SCALE, &scale, sizeof(SQLSMALLINT), &ind, "S");
	check_int(scale, ==, 0);

	CHKDescribeParam(3, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_NUMERIC);
	check_int(size, ==, 17);
	check_int(digits, ==, 5);
	check_int(nullable, ==, SQL_NULLABLE);
	CHKGetDescField(ipd, 3, SQL_DESC_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_NUMERIC);
	CHKGetDescField(ipd, 3, SQL_DESC_CONCISE_TYPE, &sql_type, sizeof(SQLSMALLINT), &ind, "S");
	check_type(sql_type, ==, SQL_NUMERIC);
	CHKGetDescField(ipd, 3, SQL_DESC_LENGTH, &size, sizeof(SQLULEN), &ind, "S");
	check_int(size, ==, 17);
	CHKGetDescField(ipd, 3, SQL_DESC_PRECISION, &digits, sizeof(SQLSMALLINT), &ind, "S");
	check_int(digits, ==, 17);
	CHKGetDescField(ipd, 3, SQL_DESC_SCALE, &scale, sizeof(SQLSMALLINT), &ind, "S");
	check_int(scale, ==, 5);

	CHKDescribeParam(4, &sql_type, &size, &digits, &nullable, "E");

	/* check parameters were filled on IPD */
	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 3);
	/* APD is not filled */
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/*****************************************************************/

	/* preparing another query  */
	CHKPrepare(T("INSERT INTO describe(i) VALUES(?)"), SQL_NTS, "S");

	/* we prepared a query with 1 parameter, we should have 1 parameter */
	CHKNumParams(&num_params, "S");
	check_int(num_params, ==, 1);

	CHKPrepare(T("INSERT INTO describe(i, vc) VALUES(?, ?)"), SQL_NTS, "S");

	/* check we have no parameters on IPD and APD */
	/* SQLPrepare should clear them */
	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	CHKNumParams(&num_params, "S");

	/* check we have no parameters on IPD and APD */
	/* SQLNumParams does not affect them */
	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_INTEGER);

	/* check parameters were filled on IPD */
	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 2);
	/* APD is not filled */
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/*****************************************************************/

	/* bind a parameter, see if affects SQLDescribeParam */
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_FLOAT, 10, 0, &id, 0, &sql_nts, "S");
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 1);

	/* what happens preparing another query ? */
	CHKPrepare(T("INSERT INTO describe(vc, i, vb) VALUES(?, ?, ?)"), SQL_NTS, "S");

	/* type do not change, set one stays */
	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_FLOAT);
	/* even this parameter remains */
	CHKDescribeParam(2, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_VARCHAR);
	/* additional parameter are not read from server */
	/* "SQL error 07009 -- [Microsoft][ODBC Driver 18 for SQL Server]Invalid Descriptor Index" */
	CHKDescribeParam(3, &sql_type, &size, &digits, &nullable, "E");

	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 2);
	/* APD remains */
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 1);

	/*****************************************************************/

	/* try to reset APD */
	CHKSetDescField(apd, 1, SQL_DESC_COUNT, (SQLPOINTER) 0, SQL_IS_SMALLINT, "S");

	CHKPrepare(T("INSERT INTO describe(i) VALUES(?)"), SQL_NTS, "S");

	/* parameter is not overridden */
	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_FLOAT);

	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 2);
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/*****************************************************************/

	/* try to reset parameters */
	CHKFreeStmt(SQL_RESET_PARAMS, "S");

	CHKPrepare(T("INSERT INTO describe(i) VALUES(?)"), SQL_NTS, "S");

	/* parameter is overridden */
	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_INTEGER);

	CHKGetDescField(ipd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 1);
	CHKGetDescField(apd, 0, SQL_DESC_COUNT, &count, sizeof(count), &ind, "S");
	check_int(count, ==, 0);

	/*****************************************************************/

	/* check what happens if connection is busy (no MARS, pending data) */
	env = getenv("ODBC_MARS");
	if (!env || atoi(env) == 0) {
		SQLHSTMT stmt;

		CHKAllocStmt(&stmt, "S");

		SWAP_STMT(stmt);
		CHKExecDirect(T("SELECT * FROM sysobjects"), SQL_NTS, "S");

		SWAP_STMT(stmt);
		CHKPrepare(T("INSERT INTO describe(vc) VALUES(?)"), SQL_NTS, "S");

		CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "E");
		odbc_read_error();
		if (strcmp(odbc_sqlstate, "07009") != 0) {
			fprintf(stderr, "Unexpected sql state returned: %s\n", odbc_sqlstate);
			odbc_disconnect();
			exit(1);
		}

		SWAP_STMT(stmt);
		CHKMoreResults("SNo");
		CHKMoreResults("No");
		CHKFreeStmt(SQL_DROP, "S");

		SWAP_STMT(stmt);
	}

	/*****************************************************************/

	/* prepare a stored procedure */
	CHKPrepare(T("{?=call sp_tables(?)}"), SQL_NTS, "S");

	CHKDescribeParam(1, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_INTEGER);
	check_int(size, ==, 10);
	check_int(digits, ==, 0);
	check_int(nullable, ==, SQL_NULLABLE);
	CHKDescribeParam(2, &sql_type, &size, &digits, &nullable, "S");
	check_type(sql_type, ==, SQL_WVARCHAR);
	check_int(size, ==, 384);
	check_int(digits, ==, 0);
	check_int(nullable, ==, SQL_NULLABLE);

	/*****************************************************************/

	/* cleanup */
	odbc_command("DROP TABLE describe");

	odbc_disconnect();
	return 0;
}
