#include "common.h"

/* some tests on error reporting */

static char software_version[] = "$Id: error.c,v 1.7 2008-11-06 15:56:39 freddy77 Exp $";
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
	SQLRETURN RetCode;
	HSTMT stmt, tmp_stmt;

	Connect();

	/* create a test table */
	Command("create table #tmp (i int)");
	Command("insert into #tmp values(3)");
	Command("insert into #tmp values(4)");
	Command("insert into #tmp values(5)");
	Command("insert into #tmp values(6)");
	Command("insert into #tmp values(7)");

	/* issue our command */
	if (db_is_microsoft())
		CHKR(CommandWithResult, (Statement, "select 100 / (i - 5) from #tmp order by i"), "SE");
	else
		CHKR(CommandWithResult, (Statement, "select 100 / (i - 5) from #tmp order by i"), "E");

	/* special case, Sybase detect error early */
	if (RetCode != SQL_ERROR) {

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

	Command("SELECT * FROM sysobjects");

	/* a statement is already active so you get error */
	CHKR(CommandWithResult, (stmt, "SELECT * FROM sysobjects"), "E");

	tmp_stmt = Statement;
	Statement = stmt;

	ReadError();

	Disconnect();

	printf("Done.\n");
	return 0;
}
