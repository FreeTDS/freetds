/*
 * Verify that empty non-NULL DAE character input is not treated as
 * SQL NULL when converted to SQL_INTEGER.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(0);
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT 1 WHERE COALESCE(?, -1) = 0"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 0, &ind, "S");

	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	/* Empty source, but explicitly not SQL_NULL_DATA. */
	CHKPutData("", 0, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
