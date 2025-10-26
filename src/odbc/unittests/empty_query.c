#include "common.h"

/* Check that on queries returning 0 rows and NOCOUNT active SQLExecDirect returns success.
 * Also SQLFetch should return NO_DATA for these queries. */

TEST_MAIN()
{
	odbc_use_version3 = true;
	odbc_connect();

	odbc_command2("SET NOCOUNT ON\nSELECT 123 AS foo WHERE 0=1", "S");
	CHKFetch("No");
	CHKMoreResults("No");

	odbc_disconnect();
	return 0;
}
