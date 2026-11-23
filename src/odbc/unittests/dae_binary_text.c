/*
 * Verify that SQL_C_BINARY input to a character DAE parameter retains
 * the established byte-preserving streaming behavior.
 *
 * If the completed value is incorrectly passed through buffered
 * binary-to-character conversion, the bytes "ABCD" become the
 * hexadecimal text "41424344" and the query returns no rows.
 */

#include "common.h"

TEST_MAIN()
{
	const unsigned char value[] = {
		(unsigned char) 'A',
		(unsigned char) 'B',
		(unsigned char) 'C',
		(unsigned char) 'D'
	};
	SQLLEN ind = SQL_LEN_DATA_AT_EXEC(sizeof(value));
	char token_value;
	SQLPOINTER token = &token_value;
	SQLPOINTER returned_token = NULL;

	odbc_connect();
	CHKAllocStmt(&odbc_stmt, "S");
	CHKPrepare(T("SELECT 1 WHERE ? = 'ABCD'"), SQL_NTS, "S");
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARCHAR, sizeof(value), 0, token, sizeof(value), &ind, "S");
	CHKExecute("Ne");

	returned_token = NULL;
	CHKParamData(&returned_token, "Ne");
	if (returned_token != token) {
		fprintf(stderr, "Unexpected parameter token: expected %p, got %p\n", token, returned_token);
		return 1;
	}

	/*
	 * Supply the binary representation in two chunks. The character
	 * destination must receive the original four bytes rather than
	 * the eight hexadecimal characters "41424344".
	 */
	CHKPutData((SQLPOINTER) value, 2, "S");
	CHKPutData((SQLPOINTER) (value + 2), 2, "S");

	returned_token = NULL;
	CHKParamData(&returned_token, "S");

	/*
	 * One row proves that the parameter retained the original "ABCD"
	 * byte representation. Hexadecimal expansion would not match.
	 */
	CHKFetch("S");
	CHKFetch("No");

	CHKFreeStmt(SQL_DROP, "S");
	odbc_disconnect();
	printf("Done.\n");
	return 0;
}
