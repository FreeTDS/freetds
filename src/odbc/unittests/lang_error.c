#include "common.h"

/* Test if SQLExecDirect return error if a error in row is returned */

static char software_version[] = "$Id: lang_error.c,v 1.5 2008-12-03 12:55:52 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	Connect();

	/* issue print statement and test message returned */
	Command2("SELECT DATEADD(dd,-100000,getdate())", "E");

	Disconnect();

	printf("Done.\n");
	return 0;
}
