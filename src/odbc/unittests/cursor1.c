#include "common.h"

/* Test cursors */

static char software_version[] = "$Id: cursor1.c,v 1.17 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define SWAP_STMT(b) do { SQLHSTMT xyz = Statement; Statement = b; b = xyz; } while(0)

static int mssql2005 = 0;

static void
CheckNoRow(const char *query)
{
	SQLRETURN RetCode;

	CHKExecDirect((SQLCHAR *) query, SQL_NTS, "SINo");
	if (RetCode == SQL_NO_DATA)
		return;

	do {
		SQLSMALLINT cols;

		CHKNumResultCols(&cols, "S");
		if (cols != 0) {
			fprintf(stderr, "Data not expected here, query:\n\t%s\n", query);
			Disconnect();
			exit(1);
		}
	} while (CHKMoreResults("SNo") == SQL_SUCCESS);
}

static void
Test0(int use_sql, const char *create_sql, const char *insert_sql, const char *select_sql)
{
#define ROWS 4
#define C_LEN 10

	SQLUINTEGER n[ROWS];
	char c[ROWS][C_LEN];
	SQLLEN c_len[ROWS], n_len[ROWS];

	SQLUSMALLINT statuses[ROWS];
	SQLUSMALLINT i;
	SQLULEN num_row;
	SQLHSTMT stmt2;
	SQLRETURN RetCode;

	/* create test table */
	Command(Statement, "IF OBJECT_ID('tempdb..#test') IS NOT NULL DROP TABLE #test");
	Command(Statement, create_sql);
	for (i = 1; i <= 6; ++i) {
		char sql_buf[80], data[10];
		memset(data, 'a' + (i - 1), sizeof(data));
		data[i] = 0;
		sprintf(sql_buf, insert_sql, data, i);
		Command(Statement, sql_buf);
	}

	/* set cursor options */
	ResetStatement();
	CHKSetStmtAttr(SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0, "S");
	CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) ROWS, 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROW_STATUS_PTR, (SQLPOINTER) statuses, 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROWS_FETCHED_PTR, &num_row, 0, "S");
	CHKSetCursorName((SQLCHAR *) "C1", SQL_NTS, "S");

	/* */
	CHKExecDirect((SQLCHAR *) select_sql, SQL_NTS, "S");

	/* bind some rows at a time */
	CHKBindCol(1, SQL_C_ULONG, n, 0, n_len, "S");
	CHKBindCol(2, SQL_C_CHAR, c, C_LEN, c_len, "S");

	/* allocate an additional statement */
	CHKAllocStmt(&stmt2, "S");

	while (CHKFetchScroll(SQL_FETCH_NEXT, 0, "SNo") == SQL_SUCCESS) {
		/* print, just for debug */
		for (i = 0; i < num_row; ++i)
			printf("row %d i %d c %s\n", (int) (i + 1), (int) n[i], c[i]);
		printf("---\n");

		/* delete a row */
		i = 1;
		if (i > 0 && i <= num_row) {
			if (mssql2005)
				CHKSetPos(i, use_sql ? SQL_POSITION : SQL_DELETE, SQL_LOCK_NO_CHANGE, "SI");
			else
				CHKSetPos(i, use_sql ? SQL_POSITION : SQL_DELETE, SQL_LOCK_NO_CHANGE, "S");
			if (use_sql) {
				SWAP_STMT(stmt2);
				CHKPrepare((SQLCHAR *) "DELETE FROM #test WHERE CURRENT OF C1", SQL_NTS, "S");
				CHKExecute("S");
				SWAP_STMT(stmt2);
			}
		}

		/* update another row */
		i = 2;
		if (i > 0 && i <= num_row) {
			strcpy(c[i - 1], "foo");
			c_len[i - 1] = 3;
			if (strstr(select_sql, "#a") == NULL || use_sql) {
				CHKSetPos(i, use_sql ? SQL_POSITION : SQL_UPDATE, SQL_LOCK_NO_CHANGE, "S");
			} else {
				unsigned char sqlstate[6];
				unsigned char msg[256];

				n[i - 1] = 321;
				CHKSetPos(i, use_sql ? SQL_POSITION : SQL_UPDATE, SQL_LOCK_NO_CHANGE, "E");

				CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, msg, sizeof(msg), NULL, "S");
				if (strstr((char *) msg, "Invalid column name 'c'") == NULL) {
					fprintf(stderr, "Expected message not found at line %d\n", __LINE__);
					exit(1);
				}
			}
			if (use_sql) {
				SWAP_STMT(stmt2);
				CHKPrepare((SQLCHAR *) "UPDATE #test SET c=? WHERE CURRENT OF C1", SQL_NTS, "S");
				CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, C_LEN, 0, c[i - 1], 0, NULL, "S");
				CHKExecute("S");
				/* FIXME this is not necessary for mssql driver */
				SQLMoreResults(Statement);
				SWAP_STMT(stmt2);
			}
		}
	}

	SWAP_STMT(stmt2);
	CHKFreeStmt(SQL_DROP, "S");
	SWAP_STMT(stmt2);

	ResetStatement();

	/* test values */
	CheckNoRow("IF (SELECT COUNT(*) FROM #test) <> 4 SELECT 1");
	CheckNoRow("IF NOT EXISTS(SELECT * FROM #test WHERE i = 3 AND c = 'ccc') SELECT 1");
	CheckNoRow("IF NOT EXISTS(SELECT * FROM #test WHERE i = 4 AND c = 'dddd') SELECT 1");
	if (strstr(select_sql, "#a") == NULL || use_sql) {
		CheckNoRow("IF NOT EXISTS(SELECT * FROM #test WHERE i = 2 AND c = 'foo') SELECT 1");
		CheckNoRow("IF NOT EXISTS(SELECT * FROM #test WHERE i = 6 AND c = 'foo') SELECT 1");
	}
}

static void
Test(int use_sql)
{
	CommandWithResult(Statement, "DROP TABLE #a");
	Command(Statement, "CREATE TABLE #a(x int)");
	Command(Statement, "INSERT INTO #a VALUES(123)");
	Test0(use_sql, "CREATE TABLE #test(i int, c varchar(6))", "INSERT INTO #test(c, i) VALUES('%s', %d)", "SELECT x AS i, c FROM #test, #a");

	Test0(use_sql, "CREATE TABLE #test(i int, c varchar(6))", "INSERT INTO #test(c, i) VALUES('%s', %d)", "SELECT i, c FROM #test");
	if (db_is_microsoft()) {
		Test0(use_sql, "CREATE TABLE #test(i int identity(1,1), c varchar(6))", "INSERT INTO #test(c) VALUES('%s')", "SELECT i, c FROM #test");
		Test0(use_sql, "CREATE TABLE #test(i int primary key, c varchar(6))", "INSERT INTO #test(c, i) VALUES('%s', %d)", "SELECT i, c FROM #test");
	}
	Test0(use_sql, "CREATE TABLE #test(i int, c varchar(6))", "INSERT INTO #test(c, i) VALUES('%s', %d)", "SELECT i, c, c + 'xxx' FROM #test");
}

int
main(int argc, char *argv[])
{
	Connect();
	CheckCursor();

	if (db_is_microsoft() && db_version_int() >= 0x09000000u)
		mssql2005 = 1;

	Test(1);

	Test(0);

	Disconnect();
	return 0;
}
