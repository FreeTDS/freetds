#include "common.h"

/* some tests on error reporting */

static char software_version[] = "$Id: error.c,v 1.2 2004-03-31 18:40:54 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLCHAR output[256];
static void ReadError(void);

static void
ReadError(void)
{
	if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: %s\n", output);
}

int
main(int argc, char *argv[])
{
	SQLRETURN retcode;

	Connect();

	/* create a test table */
	Command(Statement, "create table #tmp (i int)");
	Command(Statement, "insert into #tmp values(3)");
	Command(Statement, "insert into #tmp values(4)");
	Command(Statement, "insert into #tmp values(5)");
	Command(Statement, "insert into #tmp values(6)");
	Command(Statement, "insert into #tmp values(7)");

	/* issue our command */
	retcode = CommandWithResult(Statement, "select 100 / (i - 5) from #tmp order by i");

	/* special case, Sybase detect error early */
	if (retcode != SQL_ERROR || db_is_microsoft()) {

		if (retcode != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Error in command");

		/* TODO when multiple row fetch available test for error on some columns */

		if (SQLFetch(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLFetch failed when it shouldn't");
		if (SQLFetch(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("SQLFetch failed when it shouldn't");
		if (SQLFetch(Statement) != SQL_ERROR)
			ODBC_REPORT_ERROR("SQLFetch succeed when it shouldn't");
	}

	ReadError();
	if (!strstr((char *) output, "zero")) {
		printf("Message invalid\n");
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
