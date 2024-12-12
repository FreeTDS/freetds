#include "common.h"
#include <assert.h>

#define SQL_QUERY_LENGTH 80

/* test correctly inserted data after insert */

/* I don't remember where this test came ... - freddy77 */

static void
insert_test(bool manual)
{
	SQLLEN sql_nts = SQL_NTS;
	SQLINTEGER id = 0;
	char string[64];
	SQLINTEGER commit_off = SQL_AUTOCOMMIT_OFF;
	SQLINTEGER commit_on = SQL_AUTOCOMMIT_ON;

	if (manual)
		CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, TDS_INT2PTR(commit_off), SQL_IS_INTEGER, "SI");

	odbc_reset_statement();

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

	CHKPrepare(T("insert into test values (?, ?)"), SQL_NTS, "SI");
	for (id = 0; id < 20; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		CHKExecute("SI");
	}

	if (manual) {
		SQLEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_COMMIT);
		SQLSetConnectAttr(odbc_conn, SQL_ATTR_AUTOCOMMIT, TDS_INT2PTR(commit_on), SQL_IS_INTEGER);
	}
	odbc_reset_statement();
}

int
main(void)
{
	odbc_connect();

	odbc_command_with_result(odbc_stmt, "DROP TABLE test");
	odbc_command("CREATE TABLE test(i int, c varchar(40))");

	/* manual commit */
	insert_test(true);

	odbc_command("DELETE FROM test");

	/* auto commit */
	insert_test(false);

	odbc_command("DROP TABLE test");

	odbc_disconnect();

	return 0;
}
