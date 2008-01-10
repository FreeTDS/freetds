/* Tests 2 active statements */
#include "common.h"

static char software_version[] = "$Id: cursor3.c,v 1.4 2008-01-10 15:29:03 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLHDBC m_hdbc;

#define CHECK_RCODE(t,h,m) \
   if ( rcode != SQL_NO_DATA \
     && rcode != SQL_SUCCESS \
     && rcode != SQL_SUCCESS_WITH_INFO  \
     && rcode != SQL_NEED_DATA ) { \
      fprintf(stderr,"Error %d at: %s\n",rcode,m); \
      getErrorInfo(t,h); \
      exit(1); \
   }

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
exec_direct(int check, const char *stmt)
{
	SQLRETURN rcode;
	SQLHSTMT stmth = 0;

	rcode = SQLAllocHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hdbc, (SQLHANDLE *) & stmth);
	CHECK_RCODE(SQL_HANDLE_STMT, stmth, "SQLAllocHandle");
	rcode = SQLExecDirect(stmth, (SQLCHAR *) stmt, SQL_NTS);
	if (check) {
		CHECK_RCODE(SQL_HANDLE_STMT, stmth, "SQLExecDirect");
	}
	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) stmth);
	CHECK_RCODE(SQL_HANDLE_STMT, stmth, "SQLFreeHandle");
}

int
main(int argc, char **argv)
{
	SQLRETURN rcode;
	SQLHSTMT m_hstmt1;
	SQLHSTMT m_hstmt2;
	char buff[64];
	SQLLEN ind;

	use_odbc_version3 = 1;
	Connect();

	CheckCursor();

	m_hdbc = Connection;

	exec_direct(1, "CREATE TABLE #t1 ( k INT, c VARCHAR(20))");
	exec_direct(1, "INSERT INTO #t1 VALUES (1, 'aaa')");
	exec_direct(1, "INSERT INTO #t1 VALUES (2, 'bbbbb')");
	exec_direct(1, "INSERT INTO #t1 VALUES (3, 'ccccccccc')");
	exec_direct(1, "INSERT INTO #t1 VALUES (4, 'dd')");

	m_hstmt1 = NULL;
	rcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_DBC, m_hdbc, "SQLAllocHandle 1");

	m_hstmt2 = NULL;
	rcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_DBC, m_hdbc, "SQLAllocHandle 2");

/*
	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT,m_hstmt1,"Set attribute SQL_ATTR_CURSOR_SCROLLABLE 1");
*/

	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "Set attribute SQL_ATTR_CURSOR_SENSITIVITY 1");

/*
	rcode = SQLSetStmtAttr(m_hstmt2, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT,m_hstmt2,"Set attribute SQL_ATTR_CURSOR_SCROLLABLE 2");
*/

	rcode = SQLSetStmtAttr(m_hstmt2, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "Set attribute SQL_ATTR_CURSOR_SENSITIVITY 2");

/*
	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT,m_hstmt1,"Set attribute SQL_ATTR_CONCURRENCY 1");

	rcode = SQLSetStmtAttr(m_hstmt2, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT,m_hstmt2,"Set attribute SQL_ATTR_CONCURRENCY 2");
*/

	rcode = SQLSetCursorName(m_hstmt1, (SQLCHAR *) "c1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SetCursorName c1");

	rcode = SQLSetCursorName(m_hstmt2, (SQLCHAR *) "c2", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SetCursorName c2");

	rcode = SQLPrepare(m_hstmt1, (SQLCHAR *) "SELECT * FROM #t1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "Prepare 1");

	rcode = SQLPrepare(m_hstmt2, (SQLCHAR *) "SELECT * FROM #t1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "Prepare 2");

	rcode = SQLExecute(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecute 1");

	rcode = SQLExecute(m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLExecute 2");

	rcode = SQLFetch(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFetch 1");

	rcode = SQLGetData(m_hstmt1, 2, SQL_C_CHAR, (SQLPOINTER) buff, sizeof(buff), &ind);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLGetData 1");
	fprintf(stdout, ">> Fetch from 1: [%s]\n", buff);

	rcode = SQLFetch(m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLFetch 2");

	rcode = SQLGetData(m_hstmt2, 2, SQL_C_CHAR, (SQLPOINTER) buff, sizeof(buff), &ind);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLGetData 2");
	fprintf(stdout, ">> Fetch from 2: [%s]\n", buff);

	rcode = SQLCloseCursor(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLCloseCursor 1");

	rcode = SQLCloseCursor(m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLCloseCursor 2");

	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFreeHandle 1");

	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt2);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt2, "SQLFreeHandle 2");

	Disconnect();

	return 0;
}

