/*
 * Verify that empty non-NULL DAE character input is not treated as
 * SQL NULL when converted to SQL_INTEGER.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(0);
	SQLCHAR state[6];
	SQLCHAR message[512];
	SQLINTEGER native_error;
	SQLSMALLINT message_length;
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 0, &ind, "S");

	CHKExecute("Ne");
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	/* Empty source, but explicitly not SQL_NULL_DATA. */
	CHKPutData("", 0, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "E");

	memset(state, 0, sizeof(state));
	memset(message, 0, sizeof(message));

	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, state, &native_error, message, sizeof(message), &message_length, "S");

	if (strcmp((const char *) state, "22018") != 0) {
		fprintf(stderr, "Expected 22018 for empty integer input, got %s: %s\n",
			(const char *) state, (const char *) message);
		return 1;
	}

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
