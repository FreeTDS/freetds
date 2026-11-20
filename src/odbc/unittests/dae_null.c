/*
 * Verify that SQL_NULL_DATA supplied through SQLPutData() remains
 * SQL NULL and is not converted as an empty source value.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind;
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	odbc_command("CREATE TABLE #dae_null (id INT NOT NULL PRIMARY KEY)");
	odbc_command("INSERT INTO #dae_null VALUES (1)");

	/*
	 * Bind an SQL_C_CHAR DAE value to SQL_INTEGER. SQLPutData()
	 * subsequently marks the completed parameter as SQL NULL.
	 */
	ind = SQL_LEN_DATA_AT_EXEC(0);
	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT id FROM #dae_null WHERE ? IS NULL"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 0, &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	/*
	 * This must set column_cur_size to -1 and bypass buffered
	 * conversion. It must not append a zero-length source value.
	 */
	CHKPutData(NULL, SQL_NULL_DATA, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	/*
	 * One matching row proves that the RPC parameter remained NULL.
	 * Empty or converted non-NULL input would not satisfy IS NULL.
	 */
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
