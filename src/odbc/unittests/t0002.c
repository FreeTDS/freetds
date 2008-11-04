#include "common.h"

static char software_version[] = "$Id: t0002.c,v 1.15 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	HSTMT old_Statement;

	Connect();

	Command(Statement, "if object_id('tempdb..#odbctestdata') is not null drop table #odbctestdata");

	Command(Statement, "create table #odbctestdata (i int)");
	Command(Statement, "insert #odbctestdata values (123)");

	/*
	 * now we allocate another statement, select, get all results
	 * then make another query with first select and drop this statement
	 * result should not disappear (required for DBD::ODBC)
	 */
	old_Statement = Statement;
	Statement = SQL_NULL_HSTMT;
	CHKAllocStmt(&Statement, "S");

	Command(Statement, "select * from #odbctestdata where 0=1");

	CHKFetch("No");

	CHKCloseCursor("SI");

	Command(old_Statement, "select * from #odbctestdata");

	/* drop first statement .. data should not disappear */
	CHKFreeStmt(SQL_DROP, "S");
	Statement = old_Statement;

	CHKFetch("SI");

	CHKFetch("No");

	CHKCloseCursor("SI");

	Command(Statement, "drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
