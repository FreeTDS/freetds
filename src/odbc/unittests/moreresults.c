#include "common.h"
#include <freetds/bool.h>

/* Test for SQLMoreResults */

static void
Test(bool use_indicator)
{
	char buf[128];
	SQLLEN ind;
	SQLLEN *pind = use_indicator ? &ind : NULL;

	strcpy(buf, "I don't exist");
	ind = strlen(buf);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, buf, 128, pind, "S");

	CHKPrepare(T("SELECT id, name FROM master..sysobjects WHERE name = ?"), SQL_NTS, "S");

	CHKExecute("S");

	CHKFetch("No");

	CHKMoreResults("No");

	/* use same binding above */
	strcpy(buf, "sysobjects");
	ind = strlen(buf);

	CHKExecute("S");

	CHKFetch("S");

	CHKFetch("No");

	CHKMoreResults("No");

	ODBC_FREE();
}

TEST_MAIN()
{
	odbc_connect();

	Test(true);
	Test(false);

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
