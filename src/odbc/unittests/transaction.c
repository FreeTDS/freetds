#include "common.h"

static char software_version[] = "$Id: transaction.c,v 1.4 2004-03-06 13:03:43 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int
Test(int discard_test)
{
	int result = 0;
	SQLINTEGER rows;
	int retcode = 0;

	/* select after insert is required to test data discarding */
	char createProcedure[512];

	sprintf(createProcedure,
		"CREATE PROCEDURE testinsert @value INT AS\n"
		"INSERT INTO TestTransaction VALUES ( @value )\n%s", discard_test ? "SELECT * FROM TestTransaction\n" : "");

	/* create stored proc */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");

	result = CommandWithResult(Statement, createProcedure);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't create proc testinsert");
		retcode = 1;
		goto cleanup;
	}

	/* Start transaction */
	result = SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't start transaction");
		retcode = 1;
		goto cleanup;
	}

	/* Insert a value */
	Command(Statement, "EXEC testinsert 1");

	/* we should be able to read row count */
	if (SQLRowCount(Statement, &rows) != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't get row counts");
		retcode = 1;
		goto cleanup;
	}

	/* Commit transaction */
	result = SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_COMMIT);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't commit transaction");
		retcode = 1;
		goto cleanup;
	}

	SQLCloseCursor(Statement);

	/* Start transaction */
	result = SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't start transaction");
		retcode = 1;
		goto cleanup;
	}

	/* Insert another value */
	Command(Statement, "EXEC testinsert 2");

	/* Roll back transaction */
	result = SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_ROLLBACK);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't roll back transaction");
		retcode = 1;
		goto cleanup;
	}

	/* TODO test row inserted */

	result = SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_ON, 0);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't stop transaction");
		retcode = 1;
		goto cleanup;
	}

      cleanup:
	/* drop table */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");

	return retcode;
}

int
main(int argc, char *argv[])
{
	int result;
	int retcode = 0;

	Connect();

	/* create table */
	CommandWithResult(Statement, "DROP TABLE TestTransaction");
	result = CommandWithResult(Statement, "CREATE TABLE TestTransaction ( value INT )");
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't create table TestTransaction");
		retcode = 1;
		goto cleanup;
	}

	if (!retcode)
		retcode = Test(1);
	if (!retcode)
		retcode = Test(0);

      cleanup:
	/* drop table */
	CommandWithResult(Statement, "DROP TABLE TestTransaction");

	Disconnect();

	printf("Done.\n");
	return retcode;
}
