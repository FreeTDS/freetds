#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: insert_speed.c,v 1.7 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define SQL_QUERY_LENGTH 80

/* test correctly inserted data after insert */

/* I don't remember where this test came ... - freddy77 */

static void
insert_test_auto(void)
{
	SQLLEN sql_nts = SQL_NTS;
	SQLINTEGER id = 0;
	char string[64];

	ResetStatement();

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

	CHKPrepare((SQLCHAR *) "insert into test values (?, ?)", SQL_NTS, "SI");
	for (id = 0; id < 20; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		CHKExecute("SI");
	}

	ResetStatement();
}


static void
insert_test_man(void)
{
	SQLLEN sql_nts = SQL_NTS;
	SQLINTEGER commit_off = SQL_AUTOCOMMIT_OFF;
	SQLINTEGER commit_on = SQL_AUTOCOMMIT_ON;
	SQLINTEGER id = 0;

	char string[64];

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, int2ptr(commit_off), SQL_IS_INTEGER, "SI");

	ResetStatement();

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

	CHKPrepare((SQLCHAR *) "insert into test values (?, ?)", SQL_NTS, "SI");
	for (id = 0; id < 20; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		CHKExecute("SI");
	}

	SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_COMMIT);
	SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_on), SQL_IS_INTEGER);
	ResetStatement();
}

int
main(int argc, char **argv)
{
	Connect();

	CommandWithResult(Statement, "DROP TABLE test");
	Command(Statement, "CREATE TABLE test(i int, c varchar(40))");

	insert_test_man();

	Command(Statement, "DELETE FROM test");

	insert_test_auto();

	Command(Statement, "DROP TABLE test");

	Disconnect();

	return 0;
}
