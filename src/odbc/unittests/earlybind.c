#include "common.h"

static char software_version[] = "$Id: earlybind.c,v 1.2 2004-10-28 13:16:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER id;
	SQLLEN ind1, ind2;
	char name[64];

	Connect();

	/* bind before select */
	SQLBindCol(Statement, 1, SQL_C_SLONG, &id, sizeof(SQLINTEGER), &ind1);
	SQLBindCol(Statement, 2, SQL_C_CHAR, name, sizeof(name), &ind2);

	/* do select */
	Command(Statement, "SELECT id, name FROM sysobjects WHERE name = 'sysobjects' SELECT 123, 'foo'");

	/* get results */
	id = -1;
	memset(name, 0, sizeof(name));
	SQLFetch(Statement);

	if (id == -1 || strcmp(name, "sysobjects") != 0) {
		fprintf(stderr, "wrong results\n");
		return 1;
	}

	/* discard others data */
	SQLFetch(Statement);

	SQLMoreResults(Statement);

	id = -1;
	memset(name, 0, sizeof(name));
	SQLFetch(Statement);

	if (id != 123 || strcmp(name, "foo") != 0) {
		fprintf(stderr, "wrong results\n");
		return 1;
	}

	/* discard others data */
	SQLFetch(Statement);

	SQLMoreResults(Statement);

	/* other select */
	Command(Statement, "SELECT 321, 'minni'");

	/* get results */
	id = -1;
	memset(name, 0, sizeof(name));
	SQLFetch(Statement);

	if (id != 321 || strcmp(name, "minni") != 0) {
		fprintf(stderr, "wrong results\n");
		return 1;
	}

	/* discard others data */
	SQLFetch(Statement);

	SQLMoreResults(Statement);

	Disconnect();

	printf("Done.\n");
	return 0;
}
