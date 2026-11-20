/*
 * Verify hexadecimal SQL_C_CHAR to SQL_VARBINARY conversion when one
 * encoded byte is split across SQLPutData() calls.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(2);
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT 1 WHERE ? = 0xAB"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARBINARY, 1, 0, token, 2, &ind, "S");

	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	/*
	 * The first nibble cannot be decoded independently. The existing
	 * stream path must retain it until the second chunk arrives.
	 */
	CHKPutData("A", 1, "S");
	CHKPutData("B", 1, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
