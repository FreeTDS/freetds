#include "common.h"

/* Test if SQLExecDirect return error if a error in row is returned */

TEST_MAIN()
{
	odbc_connect();

	/* issue print statement and test message returned */
	odbc_command2("SELECT DATEADD(dd,-100000,getdate())", "E");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
