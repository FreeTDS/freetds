#include "common.h"

/*
 * Test originally written by John K. Hohm
 * This test wrong SQLFetch results with warning inside select
 * Is different from raiserror test cause in raiserror error is not
 * inside recordset
 * Sybase do not return warning but test works the same
 */
static char software_version[] = "$Id: warning.c,v 1.2 2006-06-25 08:11:31 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char one_null_with_warning[] = "select max(a) as foo from (select convert(int, null) as a) as test";

int
main(void)
{
	int res;

	Connect();

	if (SQLPrepare(Statement, (SQLCHAR *) one_null_with_warning, SQL_NTS) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to prepare statement\n");
		exit(1);
	}

	if (SQLExecute(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to execute statement\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Unable to fetch row.\n");
		CheckReturn();
		exit(1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Warning was returned as a result row -- bad!\n");
		exit(1);
	}

	/*
	 * Microsoft SQL Server 2000 provides a diagnostic record
	 * associated with the second SQLFetch (which returns
	 * SQL_NO_DATA) saying "Warning: Null value is eliminated by
	 * an aggregate or other SET operation."
	 */
	if (db_is_microsoft()) {
		SQLCHAR output[256];

		if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL))) {
			fprintf(stderr, "SQLGetDiagRec should not fail\n");
			exit(1);
		}
		printf("Message: %s\n", (char *) output);
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}

