#include "common.h"

/* test error on prepared statement, from Nathaniel Talbott test */

static char software_version[] = "$Id: preperror.c,v 1.2 2003-12-20 12:38:37 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER cbInString = SQL_NTS;
	char buf[128];

	Connect();

	Command(Statement, "CREATE TABLE #urls ( recdate DATETIME ) ");

	/* test implicit conversion error */
	if (CommandWithResult(Statement, "INSERT INTO #urls ( recdate ) VALUES ( '2003-10-1 10:11:1 0' )") != SQL_ERROR) {
		fprintf(stderr, "SQLExecDirect success instead of failing!\n");
		return 1;
	}

	/* test prepared implicit conversion error */
	if (!SQL_SUCCEEDED(SQLPrepare(Statement, (SQLCHAR *) "INSERT INTO #urls ( recdate ) VALUES ( ? )", SQL_NTS))) {
		fprintf(stderr, "SQLPrepare failure!\n");
		return 1;
	}

	strcpy(buf, "2003-10-1 10:11:1 0");
	if (!SQL_SUCCEEDED
	    (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 128, 0, buf, sizeof(buf), &cbInString))) {
		fprintf(stderr, "SQLBindParameter failure!\n");
		return 1;
	}

	if (SQLExecute(Statement) != SQL_ERROR) {
		fprintf(stderr, "SQLExecute succeeded instead of failing!\n");
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
