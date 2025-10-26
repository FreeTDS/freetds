#include "common.h"

/* Test commit/rollback with auto commit set to on (the default) */

TEST_MAIN()
{
	odbc_use_version3 = true;
	odbc_connect();

	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_COMMIT, "S");
	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_ROLLBACK, "S");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
