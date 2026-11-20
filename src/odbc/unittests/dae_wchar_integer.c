/*
 * Verify buffered SQL_C_WCHAR to SQL_INTEGER conversion.
 */

#include "common.h"

TEST_MAIN()
{
	SQLWCHAR value[] = {
		(SQLWCHAR) '1',
		(SQLWCHAR) '2',
		(SQLWCHAR) '3',
		(SQLWCHAR) '4'
	};
	SQLLEN ind;
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	odbc_command("CREATE TABLE #dae_wchar_integer (id INT NOT NULL PRIMARY KEY)");
	odbc_command("INSERT INTO #dae_wchar_integer VALUES (1234)");

	ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT id FROM #dae_wchar_integer WHERE id = ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_INTEGER, 10, 0, token, sizeof(value), &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	CHKPutData(value, 2 * sizeof(SQLWCHAR), "S");
	CHKPutData(value + 2, 2 * sizeof(SQLWCHAR), "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
