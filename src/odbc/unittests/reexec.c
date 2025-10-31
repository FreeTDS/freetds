#include "common.h"

/* Re-execute a prepared SELECT after SQLCloseCursor failed to fecth all rows */

static void
Test(bool direct)
{
	const char *sql = "SELECT a.name, b.name FROM sysobjects a, sysobjects b";
	int fetched;

	if (!direct) {
		CHKPrepare(T(sql), SQL_NTS, "S");
		CHKExecute("S");
	} else {
		CHKExecDirect(T(sql), SQL_NTS, "S");
	}

	for (fetched = 0; CHKFetch("SNo") != SQL_NO_DATA && fetched < 20; ++fetched)
		continue;
	CHKCloseCursor("SI");

	if (!direct) {
		printf("Re-executing prepared statement...\n");
		CHKExecute("S");
	} else {
		printf("Re-executing statement...\n");
		CHKExecDirect(T(sql), SQL_NTS, "S");
	}

	odbc_reset_statement();
}

TEST_MAIN()
{
	odbc_use_version3 = true;
	odbc_connect();

	Test(false);
	Test(true);

	odbc_disconnect();
	return 0;
}
