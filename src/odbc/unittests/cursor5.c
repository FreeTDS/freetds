#include "common.h"

static char software_version[] = "$Id: cursor5.c,v 1.6 2008-10-31 14:35:23 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHECK_RCODE(t,h,m) \
   if ( rcode != SQL_NO_DATA \
     && rcode != SQL_SUCCESS \
     && rcode != SQL_SUCCESS_WITH_INFO  \
     && rcode != SQL_NEED_DATA ) { \
      fprintf(stderr,"Error %d at: %s\n",rcode,m); \
      getErrorInfo(t,h); \
      exit(1); \
   }

static SQLINTEGER v_int_3;
static SQLLEN v_ind_3_1;

static char v_char_3[21];
static SQLLEN v_ind_3_2;

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLRETURN rcode = 0;
	SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
	SQLINTEGER naterror = 0;
	SQLCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	rcode = SQLGetDiagRec((SQLSMALLINT) sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      (SQLSMALLINT) 1,
			      (SQLCHAR *) sqlstate,
			      (SQLINTEGER *) & naterror,
			      (SQLCHAR *) msgtext, (SQLSMALLINT) sizeof(msgtext), (SQLSMALLINT *) & msgtextl);
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", (char *) sqlstate);
	fprintf(stderr, "  SQL code : %d\n", (int) naterror);
	fprintf(stderr, "  Message  : %s\n", (char *) msgtext);
}

static void
doFetch(int dir, int pos)
{
	SQLRETURN rcode = SQLFetchScroll(Statement, dir, pos);

	CHECK_RCODE(SQL_HANDLE_STMT, Statement, "SQLFetchScroll");
	if (rcode != SQL_NO_DATA)
		printf(">> fetch %2d %10d : %d [%s]\n", dir, pos, v_ind_3_1 ? (int) v_int_3 : -1, v_ind_3_2 ? v_char_3 : "null");
	else
		printf(">> fetch %2d %10d : no data found\n", dir, pos);
}

int
main(int argc, char **argv)
{
	SQLRETURN rcode;

	use_odbc_version3 = 1;
	Connect();
	CheckCursor();

	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));

	Command(Statement, "create table #mytab1 (k int, c char(30))");
	Command(Statement, "insert into #mytab1 values (1,'aaa')");
	Command(Statement, "insert into #mytab1 values (2,'bbb')");
	Command(Statement, "insert into #mytab1 values (3,'ccc')");

	ResetStatement();
/*	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0));	*/
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, SQL_IS_UINTEGER));

	rcode = SQLPrepare(Statement, (SQLCHAR *) "select k, c from #mytab1 order by k", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, Statement, "SQLPrepare 3");

	CHK(SQLBindCol, (Statement, 1, SQL_C_LONG, &v_int_3, 0, &v_ind_3_1));
	CHK(SQLBindCol, (Statement, 2, SQL_C_CHAR, v_char_3, sizeof(v_char_3), &v_ind_3_2));

	rcode = SQLExecute(Statement);
	CHECK_RCODE(SQL_HANDLE_STMT, Statement, "SQLExecute StmtH 3");

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

	rcode = SQLCloseCursor(Statement);
	CHECK_RCODE(SQL_HANDLE_STMT, Statement, "SQLCloseCursor StmtH 2");

	Disconnect();
	return 0;
}
