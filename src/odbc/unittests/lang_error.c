#include "common.h"

/* Test if SQLExecDirect return error if a error in row is returned */

static char software_version[] = "$Id: lang_error.c,v 1.1 2003-04-01 12:01:36 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	const char *command;

	Connect();

	/* issue print statement and test message returned */
	command = "SELECT DATEADD(dd,-100000,getdate())";
	printf("%s\n", command);
	if (SQLExecDirect(Statement, (SQLCHAR *) command, SQL_NTS) != SQL_ERROR) {
		printf("SQLExecDirect should return SQL_ERROR\n");
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
