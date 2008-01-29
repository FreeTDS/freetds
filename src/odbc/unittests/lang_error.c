#include "common.h"

/* Test if SQLExecDirect return error if a error in row is returned */

static char software_version[] = "$Id: lang_error.c,v 1.3 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	Connect();

	/* issue print statement and test message returned */
	if (CommandWithResult(Statement, "SELECT DATEADD(dd,-100000,getdate())") != SQL_ERROR) {
		fprintf(stderr, "SQLExecDirect should return SQL_ERROR\n");
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
