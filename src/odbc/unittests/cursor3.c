/* Tests 2 active statements */
#include "common.h"

static char software_version[] = "$Id: cursor3.c,v 1.7 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	SQLHSTMT stmt1 = SQL_NULL_HSTMT;
	SQLHSTMT stmt2 = SQL_NULL_HSTMT;
	SQLHSTMT old_Statement;
	char buff[64];
	SQLLEN ind;

	use_odbc_version3 = 1;
	Connect();

	CheckCursor();

	Command(Statement, "CREATE TABLE #t1 ( k INT, c VARCHAR(20))");
	Command(Statement, "INSERT INTO #t1 VALUES (1, 'aaa')");
	Command(Statement, "INSERT INTO #t1 VALUES (2, 'bbbbb')");
	Command(Statement, "INSERT INTO #t1 VALUES (3, 'ccccccccc')");
	Command(Statement, "INSERT INTO #t1 VALUES (4, 'dd')");

	old_Statement = Statement;

	CHKAllocHandle(SQL_HANDLE_STMT, Connection, &stmt1, "S");
	CHKAllocHandle(SQL_HANDLE_STMT, Connection, &stmt2, "S");


	Statement = stmt1;
/*	CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER, "S"); */
	CHKSetStmtAttr(SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER, "S");
/*	CHKSetStmtAttr(SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER, "S"); */


	Statement = stmt2;
/*	CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER, "S"); */
	CHKSetStmtAttr(SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER, "S");
/*	CHKSetStmtAttr(SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER, "S"); */

	Statement = stmt1;
	CHKSetCursorName((SQLCHAR *) "c1", SQL_NTS, "S");

	Statement = stmt2;
	CHKSetCursorName((SQLCHAR *) "c2", SQL_NTS, "S");

	Statement = stmt1;
	CHKPrepare((SQLCHAR *) "SELECT * FROM #t1", SQL_NTS, "S");

	Statement = stmt2;
	CHKPrepare((SQLCHAR *) "SELECT * FROM #t1", SQL_NTS, "S");

	Statement = stmt1;
	CHKExecute("S");

	Statement = stmt2;
	CHKExecute("S");

	Statement = stmt1;
	CHKFetch("S");

	CHKGetData(2, SQL_C_CHAR, (SQLPOINTER) buff, sizeof(buff), &ind, "S");
	printf(">> Fetch from 1: [%s]\n", buff);

	Statement = stmt2;
	CHKFetch("S");

	CHKGetData(2, SQL_C_CHAR, (SQLPOINTER) buff, sizeof(buff), &ind, "S");
	printf(">> Fetch from 2: [%s]\n", buff);

	Statement = stmt1;
	CHKCloseCursor("SI");
	Statement = stmt2;
	CHKCloseCursor("SI");

	Statement = stmt1;
	CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) stmt1, "S");
	Statement = stmt2;
	CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) stmt2, "S");

	Statement = old_Statement;
	Disconnect();

	return 0;
}

