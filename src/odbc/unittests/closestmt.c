#include "common.h"

/*
 * This test attempts to test if closing a statement with prepared query
 * success if there are a pending query on the same connection from
 * another statement.
 */

TEST_MAIN()
{
	char sql[128];
	int i;
	SQLHSTMT stmt;
	SQLINTEGER num;

	odbc_use_version3 = true;
	odbc_connect();

	/* create a table with some rows */
	odbc_command("create table #tmp (i int, c varchar(100))");
	odbc_command("insert into #tmp values(1, 'some data')");
	for (i = 0; i < 8; ++i) {
		sprintf(sql, "insert into #tmp select i+%d, c from #tmp where i <= %d", 1 << i, 1 << i);
		odbc_command(sql);
	}

	/* execute a prepared query on the connection and get all rows */
	CHKPrepare(T("select i from #tmp where i < ?"), SQL_NTS, "S");

	num = 5;
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &num, 0, NULL, "S");

	CHKExecute("S");

	for (i = 1; i < 5; ++i)
		CHKFetch("S");
	CHKFetch("No");
	CHKMoreResults("No");

	/* start getting some data from another statement */
	CHKAllocStmt(&stmt, "S");
	SWAP_STMT(stmt);

	CHKExecDirect(T("select * from #tmp"), SQL_NTS, "S");

	/* close first statement with data pending on second */
	SWAP_STMT(stmt);
	CHKFreeStmt(SQL_DROP, "S");

	SWAP_STMT(stmt);
	odbc_disconnect();
	return 0;
}
