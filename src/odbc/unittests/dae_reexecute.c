/*
 * Verify that buffered DAE conversion state is reset before each
 * execution of a prepared statement.
 */

#include "common.h"

static int
execute_integer(const char *value, SQLLEN length, SQLPOINTER token)
{
    SQLLEN ind;
    SQLPOINTER returned_token = NULL;

    ind = SQL_LEN_DATA_AT_EXEC(length);

    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, length, &ind, "S");
    CHKExecute("Ne");
    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");

    if (returned_token != token) {
        fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
        return 1;
    }

    /*
     * Split each value so stale data from a prior execution is
     * detectable.
     */
    CHKPutData((SQLPOINTER) value, 1, "S");
    CHKPutData((SQLPOINTER) (value + 1), length - 1, "S");
    returned_token = NULL;
    CHKParamData(&returned_token, "S");
    CHKFetch("S");
    CHKFetch("No");
    CHKFreeStmt(SQL_CLOSE, "S");

    return 0;
}

TEST_MAIN()
{
    char token_value1, token_value2;

    odbc_connect();
    odbc_command("CREATE TABLE #dae_reexecute (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_reexecute VALUES (1234), (5678)");

    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_reexecute WHERE id = ?"), SQL_NTS, "S");
    if (execute_integer("1234", 4, &token_value1))
        return 1;

    /*
     * The second execution must not retain "1234" or its type,
     * length, capacity, parameter index, or row index.
     */
    if (execute_integer("5678", 4, &token_value2))
        return 1;
    CHKFreeStmt(SQL_DROP, "S");

    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
