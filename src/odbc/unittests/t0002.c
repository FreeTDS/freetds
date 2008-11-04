#include "common.h"

static char software_version[] = "$Id: t0002.c,v 1.16 2008-11-04 14:46:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define SWAP_STMT(b) do { SQLHSTMT xyz = Statement; Statement = b; b = xyz; } while(0)

int
main(int argc, char *argv[])
{
	HSTMT old_Statement;

	Connect();

	Command("if object_id('tempdb..#odbctestdata') is not null drop table #odbctestdata");

	Command("create table #odbctestdata (i int)");
	Command("insert #odbctestdata values (123)");

	/*
	 * now we allocate another statement, select, get all results
	 * then make another query with first select and drop this statement
	 * result should not disappear (required for DBD::ODBC)
	 */
	old_Statement = Statement;
	Statement = SQL_NULL_HSTMT;
	CHKAllocStmt(&Statement, "S");

	Command("select * from #odbctestdata where 0=1");

	CHKFetch("No");

	CHKCloseCursor("SI");

	SWAP_STMT(old_Statement);
	Command("select * from #odbctestdata");
	SWAP_STMT(old_Statement);

	/* drop first statement .. data should not disappear */
	CHKFreeStmt(SQL_DROP, "S");
	Statement = old_Statement;

	CHKFetch("SI");

	CHKFetch("No");

	CHKCloseCursor("SI");

	Command("drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
