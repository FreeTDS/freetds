/*
 * Verify that multiple conversion-requiring DAE parameters are
 * completed sequentially using one statement-level conversion
 * context.
 */

#include "common.h"

TEST_MAIN()
{
    SQLLEN ind1, ind2;
    char token_value1, token_value2;
    SQLPOINTER token1 = &token_value1;
    SQLPOINTER token2 = &token_value2;
    SQLPOINTER returned_token = NULL;

    odbc_connect();
    odbc_command("DROP TABLE IF EXISTS #dae_multiple_params");
    odbc_command("CREATE TABLE #dae_multiple_params (id1 INT NOT NULL, id2 INT NOT NULL)");
    odbc_command("INSERT INTO #dae_multiple_params VALUES (1234, 5678)");

    ind1 = SQL_LEN_DATA_AT_EXEC(4);
    ind2 = SQL_LEN_DATA_AT_EXEC(4);

    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id1 FROM #dae_multiple_params WHERE id1 = ? AND id2 = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token1, 4, &ind1, "S");
    CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token2, 4, &ind2, "S");
    CHKExecute("Ne");

    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token1) {
        fprintf(stderr, "Unexpected first parameter token: expected %p, got %p\n", token1, returned_token);
        return 1;
    }
    CHKPutData("12", 2, "S");
    CHKPutData("34", 2, "S");

    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token2) {
        fprintf(stderr, "Unexpected second parameter token: expected %p, got %p\n", token2, returned_token);
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
