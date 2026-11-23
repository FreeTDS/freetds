/*
 * SQLCancel must terminate the incomplete DAE operation. The
 * statement must then be reusable without retaining partial data
 * from the canceled execution.
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind;
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();

	odbc_command("CREATE TABLE #dae_cancel (id INT NOT NULL PRIMARY KEY)");
	odbc_command("INSERT INTO #dae_cancel VALUES (1234)");

	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT id FROM #dae_cancel WHERE id = ?"), SQL_NTS, "S");

	ind = SQL_LEN_DATA_AT_EXEC(4);
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 4, &ind, "S");

	/* Begin a buffered conversion and leave it incomplete. */
	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	CHKPutData("12", 2, "S");

	/*
	 * SQLCancel must abandon the incomplete source buffer. The ODBC
	 * contract permits SQLExecute to be called again afterward.
	 */
	CHKCancel("S");

	/* Execute the same prepared statement again with a complete value. */
	ind = SQL_LEN_DATA_AT_EXEC(4);
	CHKExecute("Ne");
	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	CHKPutData("12", 2, "S");
	CHKPutData("34", 2, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
