#include "common.h"

/* Test SQLFetchScroll with no binded columns */

static char software_version[] = "$Id: cursor6.c,v 1.4 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int bind_all = 0;
static int normal_fetch = 0;
static int use_cursors = 1;

static void Test(void)
{
#define ROWS 5
	struct data_t {
		SQLINTEGER i;
		SQLLEN ind_i;
		char c[20];
		SQLLEN ind_c;
	} data[ROWS];
	SQLUSMALLINT statuses[ROWS];
	SQLULEN num_row;

	ResetStatement();

	/* this should not fail or return warnings */
	if (use_cursors) {
		CHKSetStmtAttr(SQL_ATTR_CONCURRENCY, int2ptr(SQL_CONCUR_READ_ONLY), 0, "S");
		CHKSetStmtAttr(SQL_ATTR_CURSOR_TYPE, int2ptr(SQL_CURSOR_STATIC), 0, "S");
	}
	CHKPrepare((SQLCHAR *) "SELECT c, i FROM #cursor6_test", SQL_NTS, "S");
	CHKExecute("S");
	CHKSetStmtAttr(SQL_ATTR_ROW_BIND_TYPE, int2ptr(sizeof(data[0])), 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE, int2ptr(ROWS), 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROW_STATUS_PTR, statuses, 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROWS_FETCHED_PTR, &num_row, 0, "S");
	if (bind_all)
		CHKBindCol(1, SQL_C_CHAR, &data[0].c, sizeof(data[0].c), &data[0].ind_c, "S");
	CHKBindCol(2, SQL_C_LONG, &data[0].i, sizeof(data[0].i), &data[0].ind_i, "S");

#define FILL(s, n) do { \
	int _n; for (_n = 0; _n < sizeof(s)/sizeof(s[0]); ++_n) s[_n] = n; \
} while(0)
	FILL(statuses, 9876);
	num_row = -3;
	data[0].i = 0xdeadbeef;
	data[1].i = 0xdeadbeef;
	if (normal_fetch)
		CHKFetch("S");
	else
		CHKFetchScroll(SQL_FETCH_NEXT, 0, "S");

	/* now check row numbers */
	printf("num_row %d statuses[0] %d statuses[1] %d odbc3 %d\n", (int) num_row,
		(int) statuses[0], (int) statuses[1], use_odbc_version3);

	if (use_odbc_version3 || !normal_fetch) {
		if (num_row != ROWS || statuses[0] != SQL_ROW_SUCCESS || statuses[1] != SQL_ROW_SUCCESS) {
			fprintf(stderr, "result error 1\n");
			exit(1);
		}
	} else {
		if (data[0].i != 1 || data[1].i != 0xdeadbeef) {
			fprintf(stderr, "result error 2\n");
			exit(1);
		}
	}

	FILL(statuses, 8765);
	num_row = -3;
	if (normal_fetch)
		CHKFetch("S");
	else
		CHKFetchScroll(SQL_FETCH_NEXT, 0, "S");
}

static void Init(void)
{
	int i;
	char sql[128];

	Command(Statement, "CREATE TABLE #cursor6_test (i INT, c VARCHAR(20))");
	for (i = 1; i <= 10; ++i) {
		sprintf(sql, "INSERT INTO #cursor6_test(i,c) VALUES(%d, 'a%db%dc%d')", i, i, i, i);
		Command(Statement, sql);
	}

}

int
main(int argc, char *argv[])
{
	use_odbc_version3 = 1;
	Connect();

	CheckCursor();

	Init();

#define ALL(n) for (n = 0; n < 2; ++n)
	ALL(use_cursors)
		ALL(bind_all)
			ALL(normal_fetch)
				Test();

	Disconnect();

	use_odbc_version3 = 0;

	Connect();
	Init();

	ALL(use_cursors)
		ALL(bind_all)
			ALL(normal_fetch)
				Test();

	Disconnect();
	
	return 0;
}
