#include "common.h"
#include <assert.h>

/* Test transaction types */

static char software_version[] = "$Id: transaction2.c,v 1.1 2008-03-07 10:57:54 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
AutoCommit(int onoff)
{
	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(onoff), 0));
}

static void
EndTransaction(SQLSMALLINT type)
{
	CHK(SQLEndTran, (SQL_HANDLE_DBC, Connection, type));
}

#define SWAP(t,a,b) do { t xyz = a; a = b; b = xyz; } while(0)
#define SWAP_CONN() do { SWAP(HENV,env,Environment); SWAP(HDBC,dbc,Connection); SWAP(HSTMT,stmt,Statement);} while(0)

static HENV env = SQL_NULL_HENV;
static HDBC dbc = SQL_NULL_HDBC;
static HSTMT stmt = SQL_NULL_HSTMT;

static int
CheckDirtyRead(void)
{
	SQLRETURN ret;

	/* transaction 1 try to change a row but not commit */
	Command(Statement, "UPDATE test_transaction SET t = 'second' WHERE n = 1");

	SWAP_CONN();

	/* second transaction try to fetch uncommited row */
	ret = CommandWithResult(Statement, "SELECT * FROM test_transaction WHERE t = 'second' AND n = 1");
	if (ret == SQL_ERROR) {
		EndTransaction(SQL_ROLLBACK);
		SWAP_CONN();
		EndTransaction(SQL_ROLLBACK);
		return 0;	/* no dirty read */
	}

	CHK(SQLFetch, (Statement));
	ret = SQLFetch(Statement);
	if (ret != SQL_NO_DATA)
		ODBC_REPORT_ERROR("other rows ??");
	SQLMoreResults(Statement);
	EndTransaction(SQL_ROLLBACK);
	SWAP_CONN();
	EndTransaction(SQL_ROLLBACK);
	return 1;
}

static int
CheckNonrepeatableRead(void)
{
	SQLRETURN ret;

	/* transaction 2 read a row */
	SWAP_CONN();
	CHK(CommandWithResult, (Statement, "SELECT * FROM test_transaction WHERE t = 'initial' AND n = 1"));
	SQLMoreResults(Statement);

	/* transaction 1 change a row and commit */
	SWAP_CONN();
	ret = CommandWithResult(Statement, "UPDATE test_transaction SET t = 'second' WHERE n = 1");
	if (ret == SQL_ERROR) {
		EndTransaction(SQL_ROLLBACK);
		SWAP_CONN();
		EndTransaction(SQL_ROLLBACK);
		SWAP_CONN();
		return 0;	/* no dirty read */
	}
	EndTransaction(SQL_COMMIT);

	SWAP_CONN();

	/* second transaction try to fetch commited row */
	Command(Statement, "SELECT * FROM test_transaction WHERE t = 'second' AND n = 1");

	CHK(SQLFetch, (Statement));
	ret = SQLFetch(Statement);
	if (ret != SQL_NO_DATA)
		ODBC_REPORT_ERROR("other rows ??");
	SQLMoreResults(Statement);
	EndTransaction(SQL_ROLLBACK);
	SWAP_CONN();
	Command(Statement, "UPDATE test_transaction SET t = 'initial' WHERE n = 1");
	EndTransaction(SQL_COMMIT);
	return 1;
}

static int
CheckPhantom(void)
{
	SQLRETURN ret;

	/* transaction 2 read a row */
	SWAP_CONN();
	CHK(CommandWithResult, (Statement, "SELECT * FROM test_transaction WHERE t = 'initial'"));
	SQLMoreResults(Statement);

	/* transaction 1 insert a row that match critera */
	SWAP_CONN();
	ret = CommandWithResult(Statement, "INSERT INTO test_transaction(n, t) VALUES(2, 'initial')");
	if (ret == SQL_ERROR) {
		EndTransaction(SQL_ROLLBACK);
		SWAP_CONN();
		EndTransaction(SQL_ROLLBACK);
		SWAP_CONN();
		return 0;	/* no dirty read */
	}
	EndTransaction(SQL_COMMIT);

	SWAP_CONN();

	/* second transaction try to fetch commited row */
	Command(Statement, "SELECT * FROM test_transaction WHERE t = 'initial'");

	CHK(SQLFetch, (Statement));
	CHK(SQLFetch, (Statement));
	ret = SQLFetch(Statement);
	if (ret != SQL_NO_DATA)
		ODBC_REPORT_ERROR("other rows ??");
	SQLMoreResults(Statement);
	EndTransaction(SQL_ROLLBACK);
	SWAP_CONN();
	Command(Statement, "DELETE test_transaction WHERE n = 2");
	EndTransaction(SQL_COMMIT);
	return 1;
}

static void
Test(int txn, const char *expected)
{
	int dirty, repeatable, phantom;
	char buf[128];

	SWAP_CONN();
	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(txn), 0));
	SWAP_CONN();

	dirty = CheckDirtyRead();
	repeatable = CheckNonrepeatableRead();
	phantom = CheckPhantom();

	sprintf(buf, "dirty %d non repeatable %d phantom %d", dirty, repeatable, phantom);
	if (strcmp(buf, expected) != 0) {
		fprintf(stderr, "detected wrong TXN\nexpected '%s' got '%s'\n", expected, buf);
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	Connect();

	/* here we can't use temporary table cause we use two connection */
	Command(Statement, "IF OBJECT_ID('test_transaction') IS NOT NULL DROP TABLE test_transaction");
	Command(Statement, "CREATE TABLE test_transaction(n NUMERIC(18,0) PRIMARY KEY, t VARCHAR(30))");
	Command(Statement, "INSERT INTO test_transaction(n, t) VALUES(1, 'initial')");

	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 2, 0));

	/* TODO test returned error */
#if 0
	/* SQL error S1009 -- Invalid argument value */
	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(SQL_TXN_REPEATABLE_READ | SQL_TXN_READ_COMMITTED), 0));
#endif

	AutoCommit(SQL_AUTOCOMMIT_OFF);

	/* save this connection and do another */
	SWAP_CONN();

	Connect();

	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 2, 0));
	AutoCommit(SQL_AUTOCOMMIT_OFF);

	SWAP_CONN();

	Test(SQL_TXN_READ_UNCOMMITTED, "dirty 1 non repeatable 1 phantom 1");
	Test(SQL_TXN_READ_COMMITTED, "dirty 0 non repeatable 1 phantom 1");
	Test(SQL_TXN_REPEATABLE_READ, "dirty 0 non repeatable 0 phantom 1");
	Test(SQL_TXN_SERIALIZABLE, "dirty 0 non repeatable 0 phantom 0");

	Disconnect();

	SWAP_CONN();

	EndTransaction(SQL_COMMIT);

	/* Sybase do not accept DROP TABLE during a transaction */
	AutoCommit(SQL_AUTOCOMMIT_ON);
	Command(Statement, "DROP TABLE test_transaction");

	Disconnect();
	return 0;
}
