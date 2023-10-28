#include "common.h"

/* test SQL_C_DEFAULT with NCHAR type */

int
main(void)
{
	char buf[102];
	SQLLEN ind;
	int failed = 0;

	odbc_use_version3 = 1;
	odbc_connect();

	CHKBindCol(1, SQL_C_DEFAULT, buf, 100, &ind, "S");
	odbc_command("SELECT CONVERT(NCHAR(10), 'Pippo 123')");

	/* get data */
	memset(buf, 0, sizeof(buf));
	CHKFetch("S");

	SQLMoreResults(odbc_stmt);
	SQLMoreResults(odbc_stmt);

	odbc_disconnect();

	/* the second string could came from Sybase configured with UTF-8 */
	if (strcmp(buf, "Pippo 123 ") != 0
	    && (odbc_db_is_microsoft() || strcmp(buf, "Pippo 123                     ") != 0)) {
		fprintf(stderr, "Wrong results '%s'\n", buf);
		failed = 1;
	}

	return failed ? 1 : 0;
}

