/*
 * Verify that SQL_RESET_PARAMS permits rebinding after a completed
 * buffered DAE parameter conversion.
 */

#include "common.h"

TEST_MAIN()
{
    SQLLEN ind;
    char token_value1, token_value2;
    SQLPOINTER token1 = &token_value1;
    SQLPOINTER token2 = &token_value2;
    SQLPOINTER returned_token = NULL;

    odbc_connect();
    odbc_command("DROP TABLE IF EXISTS #dae_reset_params");
    odbc_command("CREATE TABLE #dae_reset_params (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_reset_params VALUES (1234), (5678)");

    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_reset_params WHERE id = ?"), SQL_NTS, "S");

    ind = SQL_LEN_DATA_AT_EXEC(4);
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token1, 4, &ind, "S");
    CHKExecute("Ne");
    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token1) {
        fprintf(stderr, "Unexpected initial parameter token: expected %p, got %p\n", token1, returned_token);
        return 1;
    }
    CHKPutData("12", 2, "S");
    CHKPutData("34", 2, "S");
    returned_token = NULL;
    CHKParamData(&returned_token, "S");
    CHKFetch("S");
    CHKFetch("No");
    CHKFreeStmt(SQL_CLOSE, "S");

    CHKFreeStmt(SQL_RESET_PARAMS, "S");

    ind = SQL_LEN_DATA_AT_EXEC(4);
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token2, 4, &ind, "S");
    CHKExecute("Ne");
    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token2) {
        fprintf(stderr, "Unexpected rebound parameter token: expected %p, got %p\n", token2, returned_token);
        return 1;
    }
    CHKPutData("56", 2, "S");
    CHKPutData("78", 2, "S");
    returned_token = NULL;
    CHKParamData(&returned_token, "S");
    CHKFetch("S");
    CHKFetch("No");
    CHKFreeStmt(SQL_DROP, "S");

    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
