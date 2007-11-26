#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: insert_speed.c,v 1.6 2007-11-26 18:12:31 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define SQL_QUERY_LENGTH 80

/* test correctly inserted data after insert */

/* I don't remember where this test came ... - freddy77 */

static int
insert_test_auto(void)
{
	SQLHSTMT hstmt = NULL;
	SQLLEN sql_nts = SQL_NTS;
	char query[SQL_QUERY_LENGTH];
	SQLINTEGER id = 0;
	char string[64];

	if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, Connection, &hstmt))) {
		return (-1);
	}

	strcpy(query, "insert into test values (?, ?)");

	if (!SQL_SUCCEEDED(SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts))
	    ||
	    !SQL_SUCCEEDED(SQLBindParameter
			   (hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts))) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return (-1);
	}


	if (!SQL_SUCCEEDED(SQLPrepare(hstmt, (SQLCHAR *) query, SQL_NTS))) {
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return (-1);
	}

	for (id = 0; id < 20; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		if (!SQL_SUCCEEDED(SQLExecute(hstmt))) {
			SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			return (-1);
		}
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return (0);
}


static int
insert_test_man(void)
{
	SQLHSTMT hstmt = NULL;
	SQLLEN sql_nts = SQL_NTS;
	SQLINTEGER commit_off = SQL_AUTOCOMMIT_OFF;
	SQLINTEGER commit_on = SQL_AUTOCOMMIT_ON;
	char query[SQL_QUERY_LENGTH];
	SQLINTEGER id = 0;

	char string[64];

	if (!SQL_SUCCEEDED(SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_off), SQL_IS_INTEGER))) {
		fprintf(stderr, "Unable to set autocommit mode\n");
		return (-1);
	}

	if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, Connection, &hstmt))) {
		fprintf(stderr, "Unable to allocate statement handle\n");
		return (-1);
	}

	strcpy(query, "insert into test values (?, ?)");

	if (!SQL_SUCCEEDED(SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts))
	    ||
	    !SQL_SUCCEEDED(SQLBindParameter
			   (hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts))) {
		SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_on), SQL_IS_INTEGER);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		fprintf(stderr, "unable to bind parameters\n");
		return (-1);
	}


	if (!SQL_SUCCEEDED(SQLPrepare(hstmt, (SQLCHAR *) query, SQL_NTS))) {
		SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_on), SQL_IS_INTEGER);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		fprintf(stderr, "Unable to prepare statement\n");
		return (-1);
	}

	for (id = 0; id < 20; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		if (!SQL_SUCCEEDED(SQLExecute(hstmt))) {
			SQLEndTran(SQL_HANDLE_DBC, hstmt, SQL_ROLLBACK);
			SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_on), SQL_IS_INTEGER);
			SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			fprintf(stderr, "Unable to execute statement %d\n", (int) id);
			return (-1);
		}
	}

	SQLEndTran(SQL_HANDLE_DBC, Connection, SQL_COMMIT);
	SQLSetConnectAttr(Connection, SQL_ATTR_AUTOCOMMIT, int2ptr(commit_on), SQL_IS_INTEGER);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return (0);
}

int
main(int argc, char **argv)
{
	Connect();

	CommandWithResult(Statement, "DROP TABLE test");
	Command(Statement, "CREATE TABLE test(i int, c varchar(40))");

	if (insert_test_man() < 0)
		return 1;

	Command(Statement, "DELETE FROM test");

	if (insert_test_auto() < 0)
		return 1;

	Command(Statement, "DROP TABLE test");

	Disconnect();

	return 0;
}
