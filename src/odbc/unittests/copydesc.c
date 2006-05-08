#include "common.h"

/* Test SQLCopyDesc */

static char software_version[] = "$Id: copydesc.c,v 1.1 2006-05-08 09:39:08 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLHDESC ard, ard2;

	Connect();

	if (SQLGetStmtAttr(Statement, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLGetStmtAttr");

	if (SQLAllocHandle(SQL_HANDLE_DESC, Connection, &ard2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLAllocHandle");

	if (SQLCopyDesc(ard, ard2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLCopyDesc");

	Disconnect();

	printf("Done.\n");
	return 0;
}
