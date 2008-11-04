#include "common.h"

/* some tests on error reporting */

static char software_version[] = "$Id: error.c,v 1.5 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLCHAR output[256];
static void ReadError(void);

static void
ReadError(void)
{
	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL, "SI");
	printf("Message: %s\n", output);
}

int
main(int argc, char *argv[])
{
	SQLRETURN retcode;
	HSTMT stmt, tmp_stmt;

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

		CHKFetch("S");
		CHKFetch("S");
		CHKFetch("E");
	}

	ReadError();
	if (!strstr((char *) output, "zero")) {
		fprintf(stderr, "Message invalid\n");
		return 1;
	}

	SQLFetch(Statement);
	SQLFetch(Statement);
	SQLFetch(Statement);
	SQLMoreResults(Statement);

	CHKAllocStmt(&stmt, "S");

	Command(Statement, "SELECT * FROM sysobjects");

	CHKR(CommandWithResult, (stmt, "SELECT * FROM sysobjects"), "E");

	tmp_stmt = Statement;
	Statement = stmt;

	ReadError();

	Disconnect();

	printf("Done.\n");
	return 0;
}
