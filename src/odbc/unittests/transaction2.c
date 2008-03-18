#include "common.h"
#include <assert.h>

/* Test transaction types */

static char software_version[] = "$Id: transaction2.c,v 1.4 2008-03-18 08:22:23 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char odbc_err[256];
static char odbc_sqlstate[6];

static void
ReadError(void)
{
	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	if (!SQL_SUCCEEDED
	    (SQLGetDiagRec
	     (SQL_HANDLE_DBC, Connection, 1, (SQLCHAR *) odbc_sqlstate, NULL, (SQLCHAR *) odbc_err, sizeof(odbc_err), NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

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

static int test_with_connect = 0;

static int global_txn;

static void
my_attrs(void)
{
	CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(global_txn), 0));
	AutoCommit(SQL_AUTOCOMMIT_OFF);
}

static void
ConnectWithTxn(int txn)
{
	global_txn = txn;
	odbc_set_conn_attr = my_attrs;
	Connect();
	odbc_set_conn_attr = NULL;
}

static void
Test(int txn, const char *expected)
{
	int dirty, repeatable, phantom;
	char buf[128];

	SWAP_CONN();
	if (test_with_connect) {
		Disconnect();
		ConnectWithTxn(txn);
		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 2, 0));
	} else {
		CHK(SQLSetConnectAttr, (Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(txn), 0));
	}
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
	SQLRETURN ret;

	use_odbc_version3 = 1;
	Connect();

	/* Invalid argument value */
	ret = SQLSetConnectAttr(Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(SQL_TXN_REPEATABLE_READ | SQL_TXN_READ_COMMITTED), 0);
	ReadError();
	if (ret != SQL_ERROR || strcmp(odbc_sqlstate, "HY024") != 0) {
		Disconnect();
		fprintf(stderr, "Unexpected success\n");
		return 1;
	}

	/* here we can't use temporary table cause we use two connection */
	Command(Statement, "IF OBJECT_ID('test_transaction') IS NOT NULL DROP TABLE test_transaction");
	Command(Statement, "CREATE TABLE test_transaction(n NUMERIC(18,0) PRIMARY KEY, t VARCHAR(30))");

	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 2, 0));

	AutoCommit(SQL_AUTOCOMMIT_OFF);
	Command(Statement, "INSERT INTO test_transaction(n, t) VALUES(1, 'initial')");

#ifdef ENABLE_DEVELOPING
	/* test setting with active transaction "Operation invalid at this time" */
	ret = SQLSetConnectAttr(Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(SQL_TXN_REPEATABLE_READ), 0);
	ReadError();
	if (ret != SQL_ERROR || strcmp(odbc_sqlstate, "HY011") != 0) {
		Disconnect();
		fprintf(stderr, "Unexpected success\n");
		return 1;
	}
#endif

	EndTransaction(SQL_COMMIT);

	Command(Statement, "SELECT * FROM test_transaction");

	/* test setting with pending data */
	ret = SQLSetConnectAttr(Connection, SQL_ATTR_TXN_ISOLATION, int2ptr(SQL_TXN_REPEATABLE_READ), 0);
	ReadError();
	if (ret != SQL_ERROR || strcmp(odbc_sqlstate, "HY011") != 0) {
		Disconnect();
		fprintf(stderr, "Unexpected success\n");
		return 1;
	}

	SQLMoreResults(Statement);

	EndTransaction(SQL_COMMIT);


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

	test_with_connect = 1;

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
