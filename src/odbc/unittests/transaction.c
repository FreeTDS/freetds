#include "common.h"

static char software_version[] = "$Id: transaction.c,v 1.5 2004-03-10 17:08:56 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int
Test(int discard_test)
{
	long out_buf, out_len;
	int result = 0;
	SQLINTEGER rows;
	int retcode = 0;

      	const char *createErrorProcedure = "CREATE PROCEDURE testerror AS\n"
						"SELECT value FROM TestTransaction\n"
						"SELECT value / (value-value) FROM TestTransaction\n"
						;

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

	/* create stored proc that generates an error */
	CommandWithResult(Statement, "DROP PROCEDURE testerror");

	result = CommandWithResult(Statement, createErrorProcedure);
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("Can't create proc testerror");
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

	/* generate an error */
	result = CommandWithResult(Statement, "EXEC testerror");
	if (result != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("error: SQLExecDirect: testerror");
		retcode = 1;
		goto cleanup;
	}
	if (SQLBindCol(Statement, 1, SQL_C_SLONG , &out_buf, sizeof(out_buf), &out_len) != SQL_SUCCESS) {
		ODBC_REPORT_ERROR("error: SQLBindCol: testerror");
		goto cleanup;
	}
	do {
		while ((result = SQLFetch(Statement)) == SQL_SUCCESS) {
			printf("\t%ld\n", out_buf);
			if (out_buf != 1) {
				printf("error: expected to select '1'\n", out_buf);
				retcode = 1;
				goto cleanup;
			}
		}

		if (result != SQL_NO_DATA) {
			ODBC_REPORT_ERROR("error: SQLFetch: testerror");
			goto cleanup;
		}

		result = SQLMoreResults(Statement);
		printf("SQLMoreResults returned %d\n", result);

	} while (result == SQL_SUCCESS);

	if (result != SQL_NO_DATA) {
		ODBC_REPORT_ERROR("error: SQLMoreResults: testerror");
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
