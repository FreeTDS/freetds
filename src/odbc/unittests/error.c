#include "common.h"

/* some tests on error reporting */

static char software_version[] = "$Id: error.c,v 1.10 2010-03-02 15:07:00 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

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
	RetCode = Command2("select 100 / (i - 5) from #tmp order by i", "SE");

	/* special case, early Sybase detect error early */
	if (RetCode != SQL_ERROR) {

		/* TODO when multiple row fetch available test for error on some columns */
		CHKFetch("S");
		CHKFetch("S");
		CHKFetch("E");
	}

	ReadError();
	if (!strstr(odbc_err, "zero")) {
		fprintf(stderr, "Message invalid\n");
		return 1;
	}

	SQLFetch(Statement);
	SQLFetch(Statement);
	SQLFetch(Statement);
	SQLMoreResults(Statement);

	CHKAllocStmt(&stmt, "S");

	Command("SELECT * FROM sysobjects");

	tmp_stmt = Statement;
	Statement = stmt;

	/* a statement is already active so you get error */
	Command2("SELECT * FROM sysobjects", "E");

	ReadError();

	Disconnect();

	printf("Done.\n");
	return 0;
}
