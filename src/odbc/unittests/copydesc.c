#include "common.h"

/* Test SQLCopyDesc and SQLAllocHandle(SQL_HANDLE_DESC) */

static char software_version[] = "$Id: copydesc.c,v 1.3 2007-04-12 07:49:30 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLHDESC ard, ard2, ard3;
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

	/*
	 * this is an additional test to test additional allocation 
	 * As of 0.64 for a bug in SQLAllocDesc we only allow to allocate one
	 */
	if (SQLAllocHandle(SQL_HANDLE_DESC, Connection, &ard3) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLAllocHandle");

	if (SQLCopyDesc(ard, ard2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLCopyDesc");

	Disconnect();

	printf("Done.\n");
	return 0;
}
