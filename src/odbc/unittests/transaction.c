#include "common.h"

static char software_version[] = "$Id: transaction.c,v 1.1 2003-10-19 17:05:43 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	char buf[16];
	int result = 0;
	int retcode = 0;
	char *createProcedure = ""
		"CREATE PROCEDURE testinsert\n"
		"@value int\n" "AS\n" "insert into TestTransaction values ( @value )\n" "select * from TestTransaction\n";

	Connect();

	/* create table */
	CommandWithResult(Statement, "DROP TABLE TestTransaction");
	result = CommandWithResult(Statement, "CREATE TABLE TestTransaction ( value int )");
	if (result != SQL_SUCCESS) {
		printf("Can't create table TestTransaction\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

	/* create stored proc */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");

	result = CommandWithResult(Statement, createProcedure);
	if (result != SQL_SUCCESS) {
		printf("Can't create proc testinsert\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

	/* Start transaction */
	result = SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0);
	if (result != SQL_SUCCESS) {
		printf("Can't start transaction\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

	/* Insert a value */
	Command(Statement, "EXEC testinsert 1");

	/* Commit transaction */
	result = SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_COMMIT);
	if (result != SQL_SUCCESS) {
		printf("Can't commit transaction\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

	/* Start transaction */
	result = SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0);
	if (result != SQL_SUCCESS) {
		printf("Can't start transaction\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

	/* Insert another value */
	Command(Statement, "EXEC testinsert 2");

	/* Roll back transaction */
	result = SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_ROLLBACK);
	if (result != SQL_SUCCESS) {
		printf("Can't roll back transaction\n");
		CheckReturn();
		retcode = 1;
		goto cleanup;
	}

      cleanup:
	/* drop table */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");
	CommandWithResult(Statement, "DROP TABLE TestTransaction");

	Disconnect();

	printf("Done.\n");
	return retcode;
}
