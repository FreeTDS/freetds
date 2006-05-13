#include "common.h"

/* Test SQLCopyDesc */

static char software_version[] = "$Id: copydesc.c,v 1.2 2006-05-13 08:48:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLHDESC ard, ard2;
	SQLINTEGER id;
	SQLLEN ind1, ind2;
	char name[64];

	Connect();

	if (SQLGetStmtAttr(Statement, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLGetStmtAttr");

	SQLBindCol(Statement, 1, SQL_C_SLONG, &id, sizeof(SQLINTEGER), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, name, sizeof(name), &ind2);

	if (SQLAllocHandle(SQL_HANDLE_DESC, Connection, &ard2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLAllocHandle");

	if (SQLCopyDesc(ard, ard2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLCopyDesc");

	Disconnect();

	printf("Done.\n");
	return 0;
}
