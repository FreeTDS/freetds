#include "common.h"

TEST_MAIN()
{
	HSTMT old_odbc_stmt = SQL_NULL_HSTMT;

	odbc_connect();

	odbc_command("if object_id('tempdb..#odbctestdata') is not null drop table #odbctestdata");

	odbc_command("create table #odbctestdata (i int)");
	odbc_command("insert #odbctestdata values (123)");

	/*
	 * now we allocate another statement, select, get all results
	 * then make another query with first select and drop this statement
	 * result should not disappear (required for DBD::ODBC)
	 */
	SWAP_STMT(old_odbc_stmt);
	CHKAllocStmt(&odbc_stmt, "S");

	odbc_command("select * from #odbctestdata where 0=1");

	CHKFetch("No");

	CHKCloseCursor("SI");

	SWAP_STMT(old_odbc_stmt);
	odbc_command("select * from #odbctestdata");
	SWAP_STMT(old_odbc_stmt);

	/* drop first statement .. data should not disappear */
	CHKFreeStmt(SQL_DROP, "S");
	odbc_stmt = SQL_NULL_HSTMT;
	SWAP_STMT(old_odbc_stmt);

	CHKFetch("SI");

	CHKFetch("No");

	CHKCloseCursor("SI");

	odbc_command("drop table #odbctestdata");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
