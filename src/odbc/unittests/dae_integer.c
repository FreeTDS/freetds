/*
 * Regression test for Data-At-Execution (DAE) integer parameters.
 *
 * PDO_ODBC binds integer parameters using:
 *
 *   ValueType      = SQL_C_CHAR
 *   ParameterType  = SQL_INTEGER
 *   ColumnSize     = 10
 *   DecimalDigits  = 0
 *   StrLen_or_Ind  = SQL_LEN_DATA_AT_EXEC(1)
 *
 * FreeTDS currently returns HY090 during SQLExecute()
 * before SQL_NEED_DATA is returned.
 */

#include "common.h"

TEST_MAIN()
{
    SQLLEN ind;
    SQLPOINTER token = ((char*)0) + 0xdeadbeef;

    /* test setup */
    odbc_connect();
    odbc_command("CREATE TABLE #dae_integer (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_integer VALUES (1)");

    /*
     * According to the ODBC DAE protocol, SQLExecute()
     * should return SQL_NEED_DATA, after which the
     * application supplies the parameter via
     * SQLParamData()/SQLPutData().
     */
    ind = SQL_LEN_DATA_AT_EXEC(1);
    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_integer WHERE id = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 1, &ind, "S");
    CHKExecute("Ne");
    CHKParamData(&token, "Ne");
    CHKPutData("1", 1, "S");
    CHKParamData(&token, "S");
    CHKFetch("S");
    CHKFreeStmt(SQL_DROP, "S");

    /* cleanup */
    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
