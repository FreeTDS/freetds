#include "common.h"

static char software_version[] = "$Id: transaction.c,v 1.14 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int
Test(int discard_test)
{
	SQLINTEGER out_buf;
	SQLLEN out_len;
	int result = 0;
	SQLLEN rows;
	int retcode = 0;
	char buf[512];
	unsigned char sqlstate[6];

	const char *createErrorProcedure = "CREATE PROCEDURE testerror AS\n"
		"SELECT value FROM TestTransaction\n" "SELECT value / (value-value) FROM TestTransaction\n";

	/* select after insert is required to test data discarding */
	char createProcedure[512];

	sprintf(createProcedure,
		"CREATE PROCEDURE testinsert @value INT AS\n"
		"INSERT INTO TestTransaction VALUES ( @value )\n%s", discard_test ? "SELECT * FROM TestTransaction\n" : "");

	/* create stored proc */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");

	Command(Statement, createProcedure);

	/* create stored proc that generates an error */
	CommandWithResult(Statement, "DROP PROCEDURE testerror");

	Command(Statement, createErrorProcedure);

	/* Start transaction */
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0, "S");

	/* Insert a value */
	Command(Statement, "EXEC testinsert 1");

	/* we should be able to read row count */
	CHKRowCount(&rows, "S");

	/* Commit transaction */
	CHKEndTran(SQL_HANDLE_DBC, Connection, SQL_COMMIT, "S");

	SQLCloseCursor(Statement);

	/* Start transaction */
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0, "S");

	/* Insert another value */
	Command(Statement, "EXEC testinsert 2");

	/* Roll back transaction */
	CHKEndTran(SQL_HANDLE_DBC, Connection, SQL_ROLLBACK, "S");

	/* TODO test row inserted */

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_ON, 0, "S");

	/* generate an error */
	Command(Statement, "EXEC testerror");
	CHKBindCol(1, SQL_C_SLONG, &out_buf, sizeof(out_buf), &out_len, "S");

	while ((result = SQLFetch(Statement)) == SQL_SUCCESS) {
		printf("\t%ld\n", (long int) out_buf);
		if (out_buf != 1) {
			fprintf(stderr, "error: expected to select 1 got %ld\n", (long int) out_buf);
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

	if (result != SQL_ERROR) {
		fprintf(stderr, "SQLMoreResults should return error\n");
		retcode = 1;
		goto cleanup;
	}

	result = SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *)buf, sizeof(buf), NULL);
	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Error not set (line %d)\n", __LINE__);
		retcode = 1;
		goto cleanup;
	}
	printf("err=%s\n", buf);

	result = SQLMoreResults(Statement);
	printf("SQLMoreResults returned %d\n", result);
	if (result != SQL_NO_DATA) {
		fprintf(stderr, "SQLMoreResults should return error");
		retcode = 1;
		goto cleanup;
	}

      cleanup:
	/* drop table */
	CommandWithResult(Statement, "DROP PROCEDURE testinsert");
	CommandWithResult(Statement, "DROP PROCEDURE testerror");

	return retcode;
}

int
main(int argc, char *argv[])
{
	int retcode = 0;

	Connect();

	/* create table */
	CommandWithResult(Statement, "DROP TABLE TestTransaction");
	Command(Statement, "CREATE TABLE TestTransaction ( value INT )");

	if (!retcode)
		retcode = Test(1);
	if (!retcode)
		retcode = Test(0);

	/* drop table */
	CommandWithResult(Statement, "DROP TABLE TestTransaction");

	Disconnect();

	printf("Done.\n");
	return retcode;
}
