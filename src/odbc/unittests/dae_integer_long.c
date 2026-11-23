/*
 * Verify that a native SQLINTEGER value can be supplied to an
 * SQL_INTEGER parameter through the Data-At-Execution protocol.
 */

#include "common.h"

TEST_MAIN()
{
	SQLINTEGER value = 1;
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();
	odbc_command("CREATE TABLE #dae_integer_long (id INT NOT NULL PRIMARY KEY)");
	odbc_command("INSERT INTO #dae_integer_long VALUES (1)");

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT id FROM #dae_integer_long WHERE id = ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 10, 0, token, sizeof(value), &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}
	CHKPutData(&value, sizeof(value), "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
