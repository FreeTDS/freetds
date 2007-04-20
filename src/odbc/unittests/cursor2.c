#include "common.h"

/* Test cursor do not give error for statement that do not return rows  */

static char software_version[] = "$Id: cursor2.c,v 1.3 2007-04-20 13:27:14 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHK(func,params) \
	if (func params != SQL_SUCCESS) \
		ODBC_REPORT_ERROR(#func)

int
main(int argc, char *argv[])
{
	unsigned char sqlstate[6];
	unsigned char msg[256];
	SQLRETURN retcode;

	Connect();

	Command(Statement, "CREATE TABLE #cursor2_test (i INT)");

	retcode = SQLSetConnectAttr(Connection, SQL_ATTR_CURSOR_TYPE,  (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);
	if (retcode != SQL_SUCCESS) {
		CHK(SQLGetDiagRec, (SQL_HANDLE_DBC, Connection, 1, sqlstate, NULL, (SQLCHAR *) msg, sizeof(msg), NULL));
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
	Command(Statement, "DROP TABLE #cursor2_test");

	if (SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, msg, sizeof(msg), NULL) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("no warning expected");

	Disconnect();
	
	return 0;
}
