/*
 * Verify errors raised while converting buffered Data-At-Execution
 * character data to SQL_INTEGER.
 *
 * The conversion must occur after all SQLPutData() chunks have been
 * accumulated. Invalid text should report 22018, while values outside
 * the SQL_INTEGER range should report 22003.
 */

#include "common.h"

static bool
check_sqlstate(const char *expected_state)
{
    SQLCHAR state[6];
    SQLCHAR message[512];
    SQLINTEGER native_error;
    SQLSMALLINT message_length;

    memset(state, 0, sizeof(state));
    memset(message, 0, sizeof(message));

    CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, state, &native_error, message, sizeof(message), &message_length, "S");

    if (strcmp((const char *) state, expected_state) != 0) {
        fprintf(stderr, "Unexpected SQLSTATE: expected %s, got %s: %s\n", expected_state, state, message);
        return false;
    }

    printf("Received expected SQLSTATE %s: %s\n", state, message);
    return true;
}

static bool
test_conversion_error(const char *expected_state, SQLPOINTER token, const char *chunk1, SQLLEN chunk1_length, const char *chunk2, SQLLEN chunk2_length)
{
    SQLLEN total_length;
    SQLLEN indicator;
    SQLPOINTER returned_token;

    total_length = chunk1_length + chunk2_length;
    indicator = SQL_LEN_DATA_AT_EXEC(total_length);

    CHKAllocStmt(&odbc_stmt, "S");
    CHKPrepare(T("SELECT id FROM #dae_conversion_errors WHERE id = ?"), SQL_NTS, "S");
    CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, token, total_length, &indicator, "S");
    CHKExecute("Ne");

    returned_token = NULL;
    CHKParamData(&returned_token, "Ne");
    if (returned_token != token) {
        fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
        CHKFreeStmt(SQL_DROP, "S");
        return false;
    }

    CHKPutData((SQLPOINTER) chunk1, chunk1_length, "S");
    if (chunk2_length > 0) {
        CHKPutData((SQLPOINTER) chunk2, chunk2_length, "S");
    }

    /*
     * The complete value is converted when SQLParamData() finalizes
     * the current parameter.
     */
    returned_token = NULL;
    CHKParamData(&returned_token, "E");

    if (!check_sqlstate(expected_state)) {
        CHKFreeStmt(SQL_DROP, "S");
        return false;
    }

    CHKFreeStmt(SQL_DROP, "S");
    return true;
}

TEST_MAIN()
{
    char invalid_token_value;
    char overflow_token_value;
    char underflow_token_value;

    odbc_connect();
    odbc_command("DROP TABLE IF EXISTS #dae_conversion_errors");
    odbc_command("CREATE TABLE #dae_conversion_errors (id INT NOT NULL PRIMARY KEY)");
    odbc_command("INSERT INTO #dae_conversion_errors VALUES (1)");

    /*
     * Invalid character data: "12x".
     *
     * Splitting the input ensures that conversion cannot occur
     * successfully on the first chunk alone.
     */
    if (!test_conversion_error("22018", &invalid_token_value, "12", 2, "x", 1))
        return 1;

    /*
     * One greater than the maximum SQL_INTEGER value:
     *
     *     2147483647 + 1
     */
    if (!test_conversion_error("22003", &overflow_token_value, "21474", 5, "83648", 5))
        return 1;

    /*
     * One less than the minimum SQL_INTEGER value:
     *
     *     -2147483648 - 1
     */
    if (!test_conversion_error("22003", &underflow_token_value, "-2147", 5, "483649", 6))
        return 1;

    odbc_disconnect();
    printf("Done.\n");
    return 0;
}
