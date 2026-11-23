/*
 * Verify that SQL_C_CHAR input can be supplied to an SQL_VARCHAR
 * parameter through the Data-At-Execution protocol.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(4);
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();
	odbc_command("CREATE TABLE #dae_varchar (vc VARCHAR(20) NOT NULL)");
	odbc_command("INSERT INTO #dae_varchar VALUES ('test')");

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT vc FROM #dae_varchar WHERE vc = ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0,
		token, 4, &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}
	CHKPutData("test", 4, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
