/*
 * Control test for Data-At-Execution (DAE) varchar parameters.
 *
 * This test exercises the same DAE code path as dae_integer.c
 * but uses SQL_VARCHAR instead of SQL_INTEGER.
 *
 * Expected:
 *
 *     SQLExecute() -> SQL_NEED_DATA
 *     SQLParamData()
 *     SQLPutData()
 *     SQLParamData()
 *     SQL_SUCCESS
 */

#include "common.h"

TEST_MAIN()
{
	SQLLEN ind;
	SQLPOINTER token = ((char *) 0) + 0xdeadbeef;

	/* test setup */
	odbc_connect();
	odbc_command("DROP TABLE IF EXISTS #dae_varchar");
	odbc_command("CREATE TABLE #dae_varchar (vc VARCHAR(20) NOT NULL)");
	odbc_command("INSERT INTO #dae_varchar VALUES ('test')");

	ind = SQL_LEN_DATA_AT_EXEC(4);
	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT vc FROM #dae_varchar WHERE vc = ?"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, token, 1, &ind, "S");

	/* DAE should begin here */
	CHKExecute("Ne");
	CHKParamData(&token, "Ne");
	CHKPutData("test", 4, "S");
	CHKParamData(&token, "S");
	CHKFetch("S");
	CHKFreeStmt(SQL_DROP, "S");

	/* cleanup */
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
