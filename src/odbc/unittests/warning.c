#include "common.h"

static char software_version[] = "$Id: warning.c,v 1.1 2006-06-25 07:48:15 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char one_null_with_warning[] = "select max(a) as foo from (select cast(null as int) as a) as test";

int
main(void)
{
	int res;

	Connect();

	if (SQLPrepare(Statement, (SQLCHAR *) one_null_with_warning, SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	if (SQLExecute(Statement) != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO) {
		printf("Unable to fetch row.\n");
		CheckReturn();
		exit(1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Warning was returned as a result row -- bad!\n");
		exit(1);
	}

	/*
	 * Microsoft SQL Server 2000 provides a diagnostic record
	 * associated with the second SQLFetch (which returns
	 * SQL_NO_DATA) saying "Warning: Null value is eliminated by
	 * an aggregate or other SET operation."
	 */

	Disconnect();

	printf("Done.\n");
	return 0;
}

