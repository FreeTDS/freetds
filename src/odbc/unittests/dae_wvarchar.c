/*
 * Verify that SQL_C_WCHAR to SQL_WVARCHAR remains on the existing
 * variable-length DAE path.
 */

#include "common.h"

TEST_MAIN()
{
	SQLWCHAR value[] = {
		(SQLWCHAR) 'A',
		(SQLWCHAR) 'B',
		(SQLWCHAR) 'C',
		(SQLWCHAR) 'D'
	};
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT 1 WHERE ? = N'ABCD'"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 4, 0, token, sizeof(value), &ind, "S");

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
