/* Test sp_cursorprepare / sp_cursorexecute usage to support SELECT FOR UPDATE
 * This test compiles and works fine with SQL Server Native Client, and uses
 * the sp_cursor* AIP Server Cursors ...
 */

#include "common.h"

static char software_version[] = "$Id: cursor4.c,v 1.8 2008-11-04 14:46:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
exec_direct(const char *stmt)
{
	SQLHSTMT Statement = SQL_NULL_HSTMT;

	CHKAllocHandle(SQL_HANDLE_STMT, (SQLHANDLE) Connection, (SQLHANDLE *) & Statement, "S");
	Command(stmt);
	CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) Statement, "S");
}

int
main(int argc, char **argv)
{
	char buff[64];
	SQLLEN ind;

	use_odbc_version3 = 1;
	Connect();

	CheckCursor();

	exec_direct("CREATE TABLE #t1 ( k INT, c VARCHAR(20))");
	exec_direct("INSERT INTO #t1 VALUES (1, 'aaa')");

	ResetStatement();

	CHKSetStmtAttr(SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER, "S");

	CHKSetCursorName((SQLCHAR *) "c112", SQL_NTS, "S");

	CHKPrepare((SQLCHAR *) "SELECT * FROM #t1 FOR UPDATE", SQL_NTS, "S");

	exec_direct("BEGIN TRANSACTION");

	CHKExecute("S");

	CHKFetch("S");

	exec_direct("UPDATE #t1 SET c = 'xxx' WHERE CURRENT OF c112");

	CHKCloseCursor("SI");

	exec_direct("COMMIT TRANSACTION");

	CHKExecDirect((SQLCHAR *) "SELECT c FROM #t1 WHERE k = 1", SQL_NTS, "S");

	CHKFetch("S");

	CHKGetData(1, SQL_C_CHAR, buff, sizeof(buff), &ind, "S");

	printf(">> New value after update = [%s] (should be [xxx]) \n", buff);

	CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) Statement, "S");
	Statement = SQL_NULL_HSTMT;

	Disconnect();

	return 0;
}
