#include "common.h"

static char software_version[] = "$Id: cursor5.c,v 1.3 2008-01-10 15:29:03 freddy77 Exp $";
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
doFetch(SQLHSTMT m_hstmt, int dir, int pos)
{
	SQLRETURN rcode = SQLFetchScroll(m_hstmt, dir, pos);

	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLFetchScroll");
	if (rcode != SQL_NO_DATA)
		fprintf(stdout, ">> fetch %2d %10d : %d [%s]\n", dir, pos, v_ind_3_1 ? (int) v_int_3 : -1, v_ind_3_2 ? v_char_3 : "null");
	else
		fprintf(stdout, ">> fetch %2d %10d : no data found\n", dir, pos);
}

int
main(int argc, char **argv)
{
	SQLRETURN rcode;
	SQLHDBC m_hdbc;
	SQLHSTMT m_hstmt1;
	SQLHSTMT m_hstmt2;

	use_odbc_version3 = 1;
	Connect();
	CheckCursor();
	m_hdbc = Connection;

	rcode = SQLSetConnectAttr(m_hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_ENV, m_hdbc, "SQLSetConnectAttr(autocommit)");

	m_hstmt1 = NULL;
	rcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_DBC, m_hdbc, "SQLAllocHandle StmtH 1");

	m_hstmt2 = NULL;
	rcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_DBC, m_hdbc, "SQLAllocHandle StmtH 2");

	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "create table #mytab1 (k int, c char(30))", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect 1.1");

	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "insert into #mytab1 values (1,'aaa')", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect 1.2");
	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "insert into #mytab1 values (2,'bbb')", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect 1.3");
	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "insert into #mytab1 values (3,'ccc')", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect 1.4");

/*
	rcode = SQLSetStmtAttr(m_hstmt2, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_STATIC, 0);
	CHECK_RCODE(SQL_HANDLE_STMT,m_hstmt2,"SQLSetStmtAttr 1");
*/

	rcode = SQLSetStmtAttr(m_hstmt2, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLSetStmtAttr 1");

	rcode = SQLPrepare(m_hstmt2, (SQLCHAR *) "select k, c from #mytab1 order by k", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLPrepare 3");

	rcode = SQLBindCol(m_hstmt2, 1, SQL_C_LONG, &v_int_3, 0, &v_ind_3_1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLBindCol 3.1");

	rcode = SQLBindCol(m_hstmt2, 2, SQL_C_CHAR, v_char_3, sizeof(v_char_3), &v_ind_3_2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLBindCol 3.2");

	rcode = SQLExecute(m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLExecute StmtH 3");

	doFetch(m_hstmt2, SQL_FETCH_LAST, 0);
	doFetch(m_hstmt2, SQL_FETCH_PRIOR, 0);
	doFetch(m_hstmt2, SQL_FETCH_PRIOR, 0);
	doFetch(m_hstmt2, SQL_FETCH_PRIOR, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_FIRST, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_NEXT, 0);
	doFetch(m_hstmt2, SQL_FETCH_ABSOLUTE, 3);
	doFetch(m_hstmt2, SQL_FETCH_RELATIVE, -2);
	doFetch(m_hstmt2, SQL_FETCH_RELATIVE, -2);
	doFetch(m_hstmt2, SQL_FETCH_RELATIVE, 5);

	rcode = SQLCloseCursor(m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLCloseCursor StmtH 2");

	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFreeHandle StmtH 1");

	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLFreeHandle StmtH 2");

	Disconnect();
	return 0;
}
