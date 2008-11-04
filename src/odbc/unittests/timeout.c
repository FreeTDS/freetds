#include "common.h"
#include <assert.h>

/* Test timeout of query */

static char software_version[] = "$Id: timeout.c,v 1.10 2008-11-04 14:46:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
AutoCommit(int onoff)
{
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, int2ptr(onoff), 0, "S");
}

static void
EndTransaction(SQLSMALLINT type)
{
	CHKEndTran(SQL_HANDLE_DBC, Connection, type, "S");
}

int
main(int argc, char *argv[])
{
	HENV env;
	HDBC dbc;
	HSTMT stmt;
	SQLRETURN ret;
	SQLINTEGER i;

	Connect();

	/* here we can't use temporary table cause we use two connection */
	CommandWithResult(Statement, "drop table test_timeout");
	Command("create table test_timeout(n numeric(18,0) primary key, t varchar(30))");
	AutoCommit(SQL_AUTOCOMMIT_OFF);

	Command("insert into test_timeout(n, t) values(1, 'initial')");
	EndTransaction(SQL_COMMIT);

	Command("update test_timeout set t = 'second' where n = 1");

	/* save this connection and do another */
	env = Environment;
	dbc = Connection;
	stmt = Statement;
	Environment = SQL_NULL_HENV;
	Connection = SQL_NULL_HDBC;
	Statement = SQL_NULL_HSTMT;

	Connect();

	AutoCommit(SQL_AUTOCOMMIT_OFF);
	CHKSetStmtAttr(SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 2, 0, "S");

	i = 1;
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &i, 0, NULL, "S");

	CHKPrepare((SQLCHAR *) "update test_timeout set t = 'bad' where n = ?", SQL_NTS, "S");
	CHKExecute("E");
	EndTransaction(SQL_ROLLBACK);

	/* TODO should return error S1T00 Timeout expired, test error message */
	ret = CommandWithResult(Statement, "update test_timeout set t = 'bad' where n = 1");
	if (ret != SQL_ERROR)
		ODBC_REPORT_ERROR("SQLExecDirect success ??");

	EndTransaction(SQL_ROLLBACK);

	Disconnect();

	Environment = env;
	Connection = dbc;
	Statement = stmt;

	EndTransaction(SQL_COMMIT);

	/* Sybase do not accept DROP TABLE during a transaction */
	AutoCommit(SQL_AUTOCOMMIT_ON);
	Command("drop table test_timeout");

	Disconnect();

	return 0;
}
