#include "common.h"

/* Test if SQLExecDirect return error if a error in row is returned */

static char software_version[] = "$Id: lang_error.c,v 1.2 2003-11-08 18:00:33 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	Connect();

	/* issue print statement and test message returned */
	if (CommandWithResult(Statement, "SELECT DATEADD(dd,-100000,getdate())") != SQL_ERROR) {
		printf("SQLExecDirect should return SQL_ERROR\n");
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
