#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0004.c,v 1.16 2008-01-29 14:30:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(int use_indicator)
{
	char buf[128];
	SQLLEN ind;
	SQLLEN *pind = use_indicator ? &ind : NULL;

	strcpy(buf, "I don't exist");
	ind = strlen(buf);

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, buf, 128, pind));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "SELECT id, name FROM master..sysobjects WHERE name = ?", SQL_NTS));

	CHK(SQLExecute, (Statement));

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Not expected another recordset\n");
		exit(1);
	}

	/* use same binding above */
	strcpy(buf, "sysobjects");
	ind = strlen(buf);

	CHK(SQLExecute, (Statement));

	CHK(SQLFetch, (Statement));

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Not expected another recordset\n");
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	Connect();

	Test(1);
	Test(0);

	Disconnect();

	printf("Done.\n");
	return 0;
}
