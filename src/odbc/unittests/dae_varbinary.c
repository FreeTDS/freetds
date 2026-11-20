/*
 * Verify that SQL_C_BINARY to SQL_VARBINARY remains on the existing
 * variable-length DAE path.
 */

#include "common.h"

TEST_MAIN()
{
	const unsigned char value[] = { 0x01, 0x23, 0x45, 0x67 };
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT 1 WHERE ? = 0x01234567"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, sizeof(value), 0, token, sizeof(value), &ind, "S");

	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	CHKPutData((SQLPOINTER) value, 2, "S");
	CHKPutData((SQLPOINTER) (value + 2), 2, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
