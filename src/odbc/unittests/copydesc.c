#include "common.h"

/* Test SQLCopyDesc and SQLAllocHandle(SQL_HANDLE_DESC) */

static char software_version[] = "$Id: copydesc.c,v 1.4 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLHDESC ard, ard2, ard3;
	SQLINTEGER id;
	SQLLEN ind1, ind2;
	char name[64];

	Connect();

	CHK(SQLGetStmtAttr, (Statement, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL));

	CHK(SQLBindCol, (Statement, 1, SQL_C_SLONG, &id, sizeof(SQLINTEGER), &ind1));
	CHK(SQLBindCol, (Statement, 2, SQL_C_CHAR, name, sizeof(name), &ind2));

	CHK(SQLAllocHandle, (SQL_HANDLE_DESC, Connection, &ard2));

	/*
	 * this is an additional test to test additional allocation 
	 * As of 0.64 for a bug in SQLAllocDesc we only allow to allocate one
	 */
	CHK(SQLAllocHandle, (SQL_HANDLE_DESC, Connection, &ard3));

	CHK(SQLCopyDesc, (ard, ard2));

	Disconnect();

	printf("Done.\n");
	return 0;
}
