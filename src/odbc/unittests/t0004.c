#include "common.h"

/* Test for SQLMoreResults */

static char software_version[] = "$Id: t0004.c,v 1.17 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(int use_indicator)
{
	char buf[128];
	SQLLEN ind;
	SQLLEN *pind = use_indicator ? &ind : NULL;

	strcpy(buf, "I don't exist");
	ind = strlen(buf);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, buf, 128, pind, "S");

	CHKPrepare((SQLCHAR *) "SELECT id, name FROM master..sysobjects WHERE name = ?", SQL_NTS, "S");

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
}

int
main(int argc, char *argv[])
{
	Connect();

	Test(1);
	Test(0);

	Disconnect();

	printf("Done.\n");
	return 0;
}
