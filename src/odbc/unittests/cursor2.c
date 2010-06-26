#include "common.h"

/* Test cursor do not give error for statement that do not return rows  */

static char software_version[] = "$Id: cursor2.c,v 1.7 2010-06-26 07:19:54 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	unsigned char sqlstate[6];
	unsigned char msg[256];
	SQLRETURN retcode;

	Connect();

	Command("CREATE TABLE #cursor2_test (i INT)");

	CheckCursor();

	ResetStatement();
	CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER, "S");

	/* this should not fail or return warnings */
	Command("DROP TABLE #cursor2_test");

	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, msg, sizeof(msg), NULL, "No");

	Disconnect();
	
	return 0;
}
