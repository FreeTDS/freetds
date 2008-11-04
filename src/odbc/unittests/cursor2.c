#include "common.h"

/* Test cursor do not give error for statement that do not return rows  */

static char software_version[] = "$Id: cursor2.c,v 1.6 2008-11-04 14:46:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	unsigned char sqlstate[6];
	unsigned char msg[256];
	SQLRETURN retcode;

	Connect();

	Command("CREATE TABLE #cursor2_test (i INT)");

	retcode = SQLSetConnectAttr(Connection, SQL_ATTR_CURSOR_TYPE,  (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);
	if (retcode != SQL_SUCCESS) {
		CHKGetDiagRec(SQL_HANDLE_DBC, Connection, 1, sqlstate, NULL, (SQLCHAR *) msg, sizeof(msg), NULL, "S");
		sqlstate[5] = 0;
		if (strcmp((const char*) sqlstate, "S1092") == 0) {
			printf("Your connection seems to not support cursors, probably you are using wrong protocol version or Sybase\n");
			Disconnect();
			exit(0);
		}
		ODBC_REPORT_ERROR("SQLSetConnectAttr");
	}

	ResetStatement();

	/* this should not fail or return warnings */
	Command("DROP TABLE #cursor2_test");

	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, msg, sizeof(msg), NULL, "No");

	Disconnect();
	
	return 0;
}
