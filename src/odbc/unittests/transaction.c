#include "common.h"

static char software_version[] = "$Id: transaction.c,v 1.17 2010-07-05 09:20:33 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int
Test(int discard_test)
{
	SQLINTEGER out_buf;
	SQLLEN out_len;
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
	odbc_command_with_result(odbc_stmt, "DROP PROCEDURE testinsert");

	odbc_command(createProcedure);

	/* create stored proc that generates an error */
	odbc_command_with_result(odbc_stmt, "DROP PROCEDURE testerror");

	odbc_command(createErrorProcedure);

	/* Start transaction */
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0, "S");

	/* Insert a value */
	odbc_command("EXEC testinsert 1");

	/* we should be able to read row count */
	CHKRowCount(&rows, "S");

	/* Commit transaction */
	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_COMMIT, "S");

	SQLCloseCursor(odbc_stmt);

	/* Start transaction */
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0, "S");

	/* Insert another value */
	odbc_command("EXEC testinsert 2");

	/* Roll back transaction */
	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_ROLLBACK, "S");

	/* TODO test row inserted */

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_ON, 0, "S");

	/* generate an error */
	odbc_command("EXEC testerror");
	CHKBindCol(1, SQL_C_SLONG, &out_buf, sizeof(out_buf), &out_len, "S");

	while (CHKFetch("SNo") == SQL_SUCCESS) {
		printf("\t%ld\n", (long int) out_buf);
		if (out_buf != 1) {
			fprintf(stderr, "error: expected to select 1 got %ld\n", (long int) out_buf);
			retcode = 1;
			goto cleanup;
		}
	}

	CHKMoreResults("E");

	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, sqlstate, NULL, (SQLCHAR *)buf, sizeof(buf), NULL, "SI");
	printf("err=%s\n", buf);

	CHKMoreResults("No");

      cleanup:
	/* drop table */
	odbc_command_with_result(odbc_stmt, "DROP PROCEDURE testinsert");
	odbc_command_with_result(odbc_stmt, "DROP PROCEDURE testerror");

	return retcode;
}

int
main(int argc, char *argv[])
{
	int retcode = 0;

	odbc_connect();

	/* create table */
	odbc_command_with_result(odbc_stmt, "DROP TABLE TestTransaction");
	odbc_command("CREATE TABLE TestTransaction ( value INT )");

	if (!retcode)
		retcode = Test(1);
	if (!retcode)
		retcode = Test(0);

	/* drop table */
	odbc_command_with_result(odbc_stmt, "DROP TABLE TestTransaction");

	odbc_disconnect();

	printf("Done.\n");
	return retcode;
}
