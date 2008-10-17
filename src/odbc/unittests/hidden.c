/* Testing result column numbers having hidden columns */
/* Test from Sebastien Flaesch */

#include "common.h"

static char software_version[] = "$Id: hidden.c,v 1.5 2008-10-17 12:21:10 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	SQLSMALLINT cnt = 0;
	int failed = 0;

	use_odbc_version3 = 1;
	Connect();

	Command(Statement, "CREATE TABLE #t1 ( k INT, c CHAR(10), vc VARCHAR(10) )");
	Command(Statement, "CREATE TABLE #tmp1 (i NUMERIC(10,0) IDENTITY PRIMARY KEY, b VARCHAR(20) NULL, c INT NOT NULL)");

	/* test hidden column with FOR BROWSE */
	ResetStatement();

	Command(Statement, "SELECT c, b FROM #tmp1");

	CHK(SQLNumResultCols, (Statement, &cnt));

	if (cnt != 2) {
		fprintf(stderr, "Wrong number of columns in result set: %d\n", (int) cnt);
		failed = 1;
	}
	ResetStatement();

	/* test hidden column with cursors*/
	CheckCursor();

	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "SELECT * FROM #t1", SQL_NTS));

	CHK(SQLExecute, (Statement));

	CHK(SQLNumResultCols, (Statement, &cnt));

	if (cnt != 3) {
		fprintf(stderr, "Wrong number of columns in result set: %d\n", (int) cnt);
		failed = 1;
	}

	Disconnect();

	return failed ? 1: 0;
}
