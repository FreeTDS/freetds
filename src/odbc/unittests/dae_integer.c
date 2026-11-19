/*
 * Regression test for conversion of a Data-At-Execution character
 * parameter to SQL_INTEGER.
 *
 * PDO_ODBC binds integer parameters as SQL_C_CHAR data.
 *
 * FreeTDS must accumulate the character data provided through
 * SQLPutData(), convert it to SQL_INTEGER, and submit a valid
 * four-byte integer RPC parameter.
 */

#include "common.h"

TEST_MAIN()
{
    SQLLEN ind;
    char token_value;
    SQLPOINTER token = &token_value;
    SQLPOINTER returned_token = NULL;

    /* test setup */
    odbc_connect();
    odbc_command("CREATE TABLE #dae_integer (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_integer VALUES (1), (1234), (-2147483648)");

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
    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token) {
        fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
        return 1;
    }
    CHKPutData("1", 1, "S");
    returned_token = NULL;
    CHKParamData(&returned_token, "S");
    CHKFetch("S");
    CHKFetch("No");
    CHKFreeStmt(SQL_DROP, "S");

    /*
     * Verify that multiple SQLPutData() chunks are accumulated
     * before conversion.
     */
    ind = SQL_LEN_DATA_AT_EXEC(4);
    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_integer WHERE id = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 4, &ind, "S");
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

    /*
     * SQL_LEN_DATA_AT_EXEC describes the source representation supplied
     * through SQLPutData(), not the converted destination size. Verify
     * that an 11-byte character value can be buffered and converted to
     * the four-byte SQL_INTEGER destination representation.
     */
    ind = SQL_LEN_DATA_AT_EXEC(11);
    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_integer WHERE id = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, 11, &ind, "S");
    CHKExecute("Ne");
    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token) {
        fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
        return 1;
    }
    CHKPutData("-2147", 5, "S");
    CHKPutData("483648", 6, "S");
    returned_token = NULL;
    CHKParamData(&returned_token, "S");
    CHKFetch("S");
    CHKFetch("No");
    CHKFreeStmt(SQL_DROP, "S");

    /* cleanup */
    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
