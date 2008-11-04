#include "common.h"

static char software_version[] = "$Id: cursor5.c,v 1.7 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLINTEGER v_int_3;
static SQLLEN v_ind_3_1;

static char v_char_3[21];
static SQLLEN v_ind_3_2;

static void
doFetch(int dir, int pos)
{
	SQLRETURN RetCode;

	CHKFetchScroll(dir, pos, "SINo");

	if (RetCode != SQL_NO_DATA)
		printf(">> fetch %2d %10d : %d [%s]\n", dir, pos, v_ind_3_1 ? (int) v_int_3 : -1, v_ind_3_2 ? v_char_3 : "null");
	else
		printf(">> fetch %2d %10d : no data found\n", dir, pos);
}

int
main(int argc, char **argv)
{
	use_odbc_version3 = 1;
	Connect();
	CheckCursor();

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER, "S");

	Command(Statement, "create table #mytab1 (k int, c char(30))");
	Command(Statement, "insert into #mytab1 values (1,'aaa')");
	Command(Statement, "insert into #mytab1 values (2,'bbb')");
	Command(Statement, "insert into #mytab1 values (3,'ccc')");

	ResetStatement();
/*	CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0, "S");	*/
	CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, SQL_IS_UINTEGER, "S");

	CHKPrepare((SQLCHAR *) "select k, c from #mytab1 order by k", SQL_NTS, "SI");

	CHKBindCol(1, SQL_C_LONG, &v_int_3, 0, &v_ind_3_1, "S");
	CHKBindCol(2, SQL_C_CHAR, v_char_3, sizeof(v_char_3), &v_ind_3_2, "S");

	CHKExecute("SI");

	doFetch(SQL_FETCH_LAST, 0);
	doFetch(SQL_FETCH_PRIOR, 0);
	doFetch(SQL_FETCH_PRIOR, 0);
	doFetch(SQL_FETCH_PRIOR, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_FIRST, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_NEXT, 0);
	doFetch(SQL_FETCH_ABSOLUTE, 3);
	doFetch(SQL_FETCH_RELATIVE, -2);
	doFetch(SQL_FETCH_RELATIVE, -2);
	doFetch(SQL_FETCH_RELATIVE, 5);

	CHKCloseCursor("SI");

	Disconnect();
	return 0;
}
