#include "common.h"

/* Test SQLFetchScroll with no bound columns */

static char software_version[] = "$Id: cursor7.c,v 1.1 2008-06-12 03:52:05 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(void)
{
	enum { ROWS=5 };
	struct data_t {
		SQLINTEGER i;
		SQLLEN ind_i;
		char c[20];
		SQLLEN ind_c;
	} data[ROWS];
	SQLUSMALLINT statuses[ROWS];
	SQLULEN num_row;

	int i;
	SQLRETURN ErrCode;

	ResetStatement();

	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CONCURRENCY, int2ptr(SQL_CONCUR_READ_ONLY), 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_CURSOR_TYPE, int2ptr(SQL_CURSOR_STATIC), 0));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "SELECT c, i FROM #cursor7_test", SQL_NTS));
	CHK(SQLExecute, (Statement));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_BIND_TYPE, int2ptr(sizeof(data[0])), 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_ARRAY_SIZE, int2ptr(ROWS), 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_STATUS_PTR, statuses, 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROWS_FETCHED_PTR, &num_row, 0));

	CHK(SQLBindCol, (Statement, 1, SQL_C_CHAR, &data[0].c, sizeof(data[0].c), &data[0].ind_c));
	CHK(SQLBindCol, (Statement, 2, SQL_C_LONG, &data[0].i, sizeof(data[0].i), &data[0].ind_i));

	/* Read records from last to first */
	printf("\n\nReading records from last to first:\n");
	ErrCode = SQLFetchScroll(Statement, SQL_FETCH_LAST, -ROWS);
	while ((ErrCode == SQL_SUCCESS) || (ErrCode == SQL_SUCCESS_WITH_INFO)) {
		/* Print this set of rows */
		for (i = ROWS - 1; i >= 0; i--) {
			if (statuses[i] != SQL_ROW_NOROW)
				printf("\t %d, %s\n", (int) data[i].i, data[i].c);
		}

		/* Read next rowset */
		ErrCode = SQLFetchScroll(Statement, SQL_FETCH_RELATIVE, -ROWS);
	}
	printf("\nRecords 5, 4 and 3 are returned twice!!\n\n");
}

static void
Init(void)
{
	int i;
	char sql[128];

	printf("\n\nCreating table #cursor7_test with 12 records.\n");

	Command(Statement, "\tCREATE TABLE #cursor7_test (i INT, c VARCHAR(20))");
	for (i = 1; i <= 12; ++i) {
		sprintf(sql, "\tINSERT INTO #cursor7_test(i,c) VALUES(%d, 'a%db%dc%d')", i, i, i, i);
		Command(Statement, sql);
	}

}

int
main(int argc, char *argv[])
{
	unsigned char sqlstate[6], msg[256];
	SQLRETURN retcode;

	use_odbc_version3 = 1;
	Connect();

	retcode = SQLSetConnectAttr(Connection, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);
	if (retcode != SQL_SUCCESS) {
		CHK(SQLGetDiagRec, (SQL_HANDLE_DBC, Connection, 1, sqlstate, NULL, (SQLCHAR *) msg, sizeof(msg), NULL));
		sqlstate[5] = 0;
		if (strcmp((const char *) sqlstate, "S1092") == 0) {
			printf("Your connection seems to not support cursors, probably you are using wrong protocol version or Sybase\n");
			Disconnect();
			exit(0);
		}
		ODBC_REPORT_ERROR("SQLSetConnectAttr");
	}

	Init();

	Test();

	Disconnect();

	return 0;
}
