#include "common.h"

/* Test cursors */

static char software_version[] = "$Id: scroll.c,v 1.4 2007-04-19 09:11:56 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHK(func,params) \
	if (func params != SQL_SUCCESS) \
		ODBC_REPORT_ERROR(#func)

int
main(int argc, char *argv[])
{
#define ROWS 3
#define C_LEN 10

	SQLUINTEGER n[ROWS];
	char c[ROWS][C_LEN];
	SQLINTEGER c_len[ROWS], n_len[ROWS];

	SQLUSMALLINT statuses[ROWS];
	SQLUSMALLINT i;
	SQLULEN num_row;
	SQLRETURN retcode;
	int i_test;

	typedef struct _test
	{
		SQLUSMALLINT type;
		SQLINTEGER irow;
		int start;
		int num;
	} TEST;

	static const TEST tests[] = {
		{SQL_FETCH_NEXT, 0, 1, 3},
		{SQL_FETCH_NEXT, 0, 4, 2},
		{SQL_FETCH_PRIOR, 0, 1, 3},
		{SQL_FETCH_NEXT, 0, 4, 2},
		{SQL_FETCH_NEXT, 0, -1, -1},
		{SQL_FETCH_FIRST, 0, 1, 3},
		{SQL_FETCH_ABSOLUTE, 3, 3, 3},
		{SQL_FETCH_RELATIVE, 1, 4, 2},
		{SQL_FETCH_LAST, 0, 3, 3}
	};
	const int num_tests = sizeof(tests) / sizeof(TEST);

	use_odbc_version3 = 1;

	Connect();

	/* create test table */
	Command(Statement, "IF OBJECT_ID('tempdb..#test') IS NOT NULL DROP TABLE #test");
	Command(Statement, "CREATE TABLE #test(i int, c varchar(6))");
	Command(Statement, "INSERT INTO #test(i, c) VALUES(1, 'a')");
	Command(Statement, "INSERT INTO #test(i, c) VALUES(2, 'bb')");
	Command(Statement, "INSERT INTO #test(i, c) VALUES(3, 'ccc')");
	Command(Statement, "INSERT INTO #test(i, c) VALUES(4, 'dddd')");
	Command(Statement, "INSERT INTO #test(i, c) VALUES(5, 'eeeee')");

	/* set cursor options */
	ResetStatement();
	retcode = SQLSetStmtAttr(Statement, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	if (retcode != SQL_SUCCESS) {
		char output[256];
		unsigned char sqlstate[6];

		CHK(SQLGetDiagRec, (SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *) output, sizeof(output), NULL));
		sqlstate[5] = 0;
		if (strcmp((const char*) sqlstate, "01S02") == 0) {
			printf("Your connection seems to not support cursors, probably you are using wrong protocol version or Sybase\n");
			exit(0);
		}
		ODBC_REPORT_ERROR("SQLSetStmtAttr");
	}


	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_SCROLLABLE, 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) ROWS, 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_STATUS_PTR, (SQLPOINTER) statuses, 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROWS_FETCHED_PTR, &num_row, 0));

	/* */
	CHK(SQLExecDirect, (Statement, (SQLCHAR *) "SELECT i, c FROM #test", SQL_NTS));

	/* bind some rows at a time */
	CHK(SQLBindCol, (Statement, 1, SQL_C_ULONG, n, 0, n_len));
	CHK(SQLBindCol, (Statement, 2, SQL_C_CHAR, c, C_LEN, c_len));

	for (i_test = 0; i_test < num_tests; ++i_test) {
		const TEST *t = &tests[i_test];

		printf("Test %d\n", i_test + 1);

		retcode = SQLFetchScroll(Statement, t->type, t->irow);
		if (retcode == SQL_SUCCESS) {
			if (t->start < 1) {
				fprintf(stderr, "Rows not expected\n");
				exit(1);
			}

			/* print, just for debug */
			for (i = 0; i < num_row; ++i)
				printf("row %d i %d c %s\n", (int) (i + 1), (int) n[i], c[i]);
			printf("---\n");

			if (num_row != t->num) {
				fprintf(stderr, "Expected %d rows, got %d\n", t->num, (int) num_row);
				exit(1);
			}

			for (i = 0; i < num_row; ++i) {
				char name[10];

				memset(name, 0, sizeof(name));
				memset(name, 'a' - 1 + i + t->start, i + t->start);
				if (n[i] != i + t->start || c_len[i] != strlen(name) || strcmp(c[i], name) != 0) {
					fprintf(stderr, "Wrong row returned\n");
					fprintf(stderr, "\tn %d %d\n", (int) n[i], i + t->start);
					fprintf(stderr, "\tc len %d %d\n", (int) c_len[i], (int) strlen(name));
					fprintf(stderr, "\tc %s %s\n", c[i], name);
					exit(1);
				}
			}
			continue;
		}

		if (retcode == SQL_NO_DATA && t->start == -1)
			continue;

		fprintf(stderr, "retcode = %d\n", retcode);
		ODBC_REPORT_ERROR("SQLFetchScroll");
	}

	ResetStatement();

	Disconnect();
	return 0;
}
