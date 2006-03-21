#include "common.h"

/* Test cursor do not give error for statement that do not return rows  */

static char software_version[] = "$Id: cursor2.c,v 1.1 2006-03-21 14:25:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHK(func,params) \
	if (func params != SQL_SUCCESS) \
		ODBC_REPORT_ERROR(#func)

int
main(int argc, char *argv[])
{
        unsigned char sqlstate[6];
        unsigned char msg[256];

	Connect();

	Command(Statement, "CREATE TABLE #cursor2_test (i INT)");

	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_CURSOR_TYPE,  (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER));

	ResetStatement();

	/* this should not fail or return warnings */
	Command(Statement, "DROP TABLE #cursor2_test");

	if (SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, msg, sizeof(msg), NULL) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("no warning expected");

	Disconnect();
	
	return 0;
}
