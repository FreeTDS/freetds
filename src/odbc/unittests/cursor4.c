/* Test sp_cursorprepare / sp_cursorexecute usage to support SELECT FOR UPDATE
 * This test compiles and works fine with SQL Server Native Client, and uses
 * the sp_cursor* AIP Server Cursors ...
 */

#include "common.h"

static char software_version[] = "$Id: cursor4.c,v 1.2 2007-12-19 15:09:55 freddy77 Exp $";
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
	char buff[64];
	SQLLEN ind;

#ifndef ENABLE_DEVELOPING
	return 0;
#endif

	use_odbc_version3 = 1;
	Connect();

	m_hdbc = Connection;

	exec_direct(0, "DROP TABLE t1");
	exec_direct(1, "CREATE TABLE t1 ( k INT, c VARCHAR(20))");
	exec_direct(1, "INSERT INTO t1 VALUES (1, 'aaa')");

	m_hstmt1 = NULL;
	rcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_DBC, m_hdbc, "SQLAllocHandle 1");

	rcode = SQLSetStmtAttr(m_hstmt1, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_LOCK, SQL_IS_UINTEGER);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "Set attribute SQL_ATTR_CONCURRENCY");

	rcode = SQLSetCursorName(m_hstmt1, (SQLCHAR *) "c112", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SetCursorName c112");

	rcode = SQLPrepare(m_hstmt1, (SQLCHAR *) "SELECT * FROM t1 FOR UPDATE", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "Prepare 2");

	exec_direct(1, "BEGIN TRANSACTION");

	rcode = SQLExecute(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecute 2");

	rcode = SQLFetch(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFetch 1");

	exec_direct(1, "UPDATE t1 SET c = 'xxx' WHERE CURRENT OF c112");

	rcode = SQLCloseCursor(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLCloseCursor 2");

	exec_direct(1, "COMMIT TRANSACTION");

	rcode = SQLExecDirect(m_hstmt1, (SQLCHAR *) "SELECT c FROM t1 WHERE k = 1", SQL_NTS);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLExecDirect 2");

	rcode = SQLFetch(m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFetch 2");

	rcode = SQLGetData(m_hstmt1, 1, SQL_C_CHAR, buff, sizeof(buff), &ind);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLGetData");

	fprintf(stdout, ">> New value after update = [%s] (should be [xxx]) \n", buff);

	rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt1);
	CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt1, "SQLFreeHandle 1");

	exec_direct(1, "DROP TABLE t1");

	Disconnect();

	return 0;
}
