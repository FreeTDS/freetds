#include "common.h"

static char software_version[] = "$Id: cursor5.c,v 1.10 2010-07-05 09:20:33 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLINTEGER v_int_3;
static SQLLEN v_ind_3_1;

static char v_char_3[21];
static SQLLEN v_ind_3_2;

static void
doFetch(int dir, int pos)
{
	SQLRETURN RetCode;

	RetCode = CHKFetchScroll(dir, pos, "SINo");

	if (RetCode != SQL_NO_DATA)
		printf(">> fetch %2d %10d : %d [%s]\n", dir, pos, v_ind_3_1 ? (int) v_int_3 : -1, v_ind_3_2 ? v_char_3 : "null");
	else
		printf(">> fetch %2d %10d : no data found\n", dir, pos);
}

int
main(int argc, char **argv)
{
	odbc_use_version3 = 1;
	odbc_connect();
	odbc_check_cursor();

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER, "S");

	odbc_command("create table #mytab1 (k int, c char(30))");
	odbc_command("insert into #mytab1 values (1,'aaa')");
	odbc_command("insert into #mytab1 values (2,'bbb')");
	odbc_command("insert into #mytab1 values (3,'ccc')");

	odbc_reset_statement();
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

	odbc_disconnect();
	return 0;
}
