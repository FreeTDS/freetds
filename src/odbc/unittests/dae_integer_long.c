/*
 * Regression test for Data-At-Execution (DAE) integer parameters.
 *
 * Verify that a native SQLINTEGER value can be supplied to an
 * SQL_INTEGER parameter through the ODBC Data-At-Execution protocol.
 *
 * Expected flow:
 *
 *   SQLExecute()   -> SQL_NEED_DATA
 *   SQLParamData() -> SQL_NEED_DATA
 *   SQLPutData()   -> SQL_SUCCESS
 *   SQLParamData() -> SQL_SUCCESS
 */
#include "common.h"

TEST_MAIN()
{
    SQLLEN ind;
    SQLINTEGER value = 1;
    SQLPOINTER token = ((char*)0) + 0xdeadbeef;

    /* test setup */
    odbc_connect();
    odbc_command("DROP TABLE IF EXISTS #dae_integer");
    odbc_command("CREATE TABLE #dae_integer (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_integer VALUES (1)");

    /*
     * According to the ODBC DAE protocol, SQLExecute()
     * should return SQL_NEED_DATA, after which the
     * application supplies the parameter via
     * SQLParamData()/SQLPutData().
     */
    ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_integer WHERE id = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 10, 0, token, sizeof(value), &ind, "S");
    CHKExecute("Ne");
    CHKParamData(&token, "Ne");
    CHKPutData(&value, sizeof(value), "S");
    CHKParamData(&token, "S");
    CHKFetch("S");
    CHKFreeStmt(SQL_DROP, "S");

    /* cleanup */
    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
