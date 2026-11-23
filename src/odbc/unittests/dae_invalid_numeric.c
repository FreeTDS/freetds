/*
 * Verify that an invalid SQL_NUMERIC_STRUCT supplied through DAE
 * reports a conversion error when SQLParamData() finalizes it.
 */

#include "common.h"

TEST_MAIN()
{
	SQL_NUMERIC_STRUCT value;
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
	SQLCHAR state[6];
	SQLCHAR message[512];
	SQLINTEGER native_error;
	SQLSMALLINT message_length;
	SQLLEN first_length;
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	memset(&value, 0, sizeof(value));

	/* convert_numeric2server() rejects precision greater than 38. */
	value.precision = 39;
	value.scale = 0;
	value.sign = 1;
	value.val[0] = 1;

	first_length = sizeof(value) / 2;

	odbc_connect();
	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC, 38, 0, token, sizeof(value), &ind, "S");

	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	CHKPutData(&value, first_length, "S");
	CHKPutData((char *) &value + first_length, sizeof(value) - first_length, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "E");

	memset(state, 0, sizeof(state));
	memset(message, 0, sizeof(message));

	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, state, &native_error, message, sizeof(message), &message_length, "S");

	if (strcmp((const char *) state, "07006") != 0) {
		fprintf(stderr, "Unexpected SQLSTATE for invalid numeric: %s: %s\n",
			(const char *) state, (const char *) message);
		return 1;
	}

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
