#include "common.h"

/* test SQL_C_DEFAULT with NCHAR type */

static char software_version[] = "$Id: wchar.c,v 1.1 2008-02-05 10:03:57 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[102];
	SQLLEN ind;
	int failed = 0;

	/* TODO remove if not neeeded */
	use_odbc_version3 = 1;
	Connect();

	SQLBindCol(Statement, 1, SQL_C_DEFAULT, buf, 100, &ind);
	Command(Statement, "SELECT CONVERT(NCHAR(10), 'Pippo 123')");

	/* get data */
	memset(buf, 0, sizeof(buf));
	SQLFetch(Statement);

	SQLMoreResults(Statement);
	SQLMoreResults(Statement);

	Disconnect();

	if (strcmp(buf, "Pippo 123 ") != 0) {
		fprintf(stderr, "Wrong results '%s'\n", buf);
		failed = 1;
	}

	return failed ? 1 : 0;
}

