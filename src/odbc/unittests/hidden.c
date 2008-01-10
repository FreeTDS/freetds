/* Testing result column numbers having hidden columns */
/* Test from Sebastien Flaesch */

#include "common.h"

static char software_version[] = "$Id: hidden.c,v 1.3 2008-01-10 21:19:47 freddy77 Exp $";
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

int
main(int argc, char **argv)
{
	SQLRETURN rcode;
	SQLHSTMT m_hstmt1;
	SQLSMALLINT cnt = 0;
	int failed = 0;

	use_odbc_version3 = 1;
	Connect();

	Command(Statement, "CREATE TABLE #t1 ( k INT, c CHAR(10), vc VARCHAR(10) )");
	Command(Statement, "CREATE TABLE #tmp1 (i NUMERIC(10,0) IDENTITY PRIMARY KEY, b VARCHAR(20) NULL, c INT NOT NULL)");

	/* test hidden column with FOR BROWSE */
	ResetStatement();

	m_hstmt1 = Statement;

	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "SELECT c, b FROM #tmp1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect");

	rcode = SQLNumResultCols(m_hstmt1, &cnt);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLNumResultCols 1");

	if (cnt != 2) {
		fprintf(stderr, "Wrong number of columns in result set: %d\n", (int) cnt);
		failed = 1;
	}
	ResetStatement();

	/* test hidden column with cursors*/
	CheckCursor();
	m_hstmt1 = Statement;

	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLSetStmtAttr SQL_ATTR_CURSOR_SCROLLABLE");
	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLSetStmtAttr SQL_ATTR_CURSOR_SENSITIVITY");

	rcode = SQLPrepare(m_hstmt1, (SQLCHAR *) "SELECT * FROM #t1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLPrepare 1");

	rcode = SQLExecute(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecute 1");

	rcode = SQLNumResultCols(m_hstmt1, &cnt);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLNumResultCols 1");

	if (cnt != 3) {
		fprintf(stderr, "Wrong number of columns in result set: %d\n", (int) cnt);
		failed = 1;
	}

	Disconnect();

	return failed ? 1: 0;
}
