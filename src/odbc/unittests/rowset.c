#include "common.h"

static char software_version[] = "$Id: rowset.c,v 1.5 2008-11-04 14:46:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char odbc_err[256];
static char odbc_sqlstate[6];

static void
ReadError(void)
{
	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, (SQLCHAR *) odbc_sqlstate, NULL, (SQLCHAR *) odbc_err, sizeof(odbc_err), NULL, "SI");
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

static void
test_err(int n)
{
	CHKSetStmtAttr(SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(n), 0, "E");
	ReadError();
	if (strcmp(odbc_sqlstate, "HY024") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		Disconnect();
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	int i;
	SQLLEN len;
#ifdef HAVE_SQLROWSETSIZE
	SQLROWSETSIZE row_count;
#else
	SQLULEN row_count;
#endif
	SQLUSMALLINT statuses[10];
	char buf[32];

	use_odbc_version3 = 1;
	Connect();

	/* initial value should be 1 */
	CHKGetStmtAttr(SQL_ROWSET_SIZE, &len, sizeof(len), NULL, "S");
	if (len != 1) {
		fprintf(stderr, "len should be 1\n");
		Disconnect();
		return 1;
	}

	/* check invalid parameter values */
	test_err(-123);
	test_err(-1);
	test_err(0);

	CheckCursor();

	/* set some correct values */
	CHKSetStmtAttr(SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(2), 0, "S");
	CHKSetStmtAttr(SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(1), 0, "S");

	/* now check that SQLExtendedFetch works as expected */
	Command("CREATE TABLE #rowset(n INTEGER, c VARCHAR(20))");
	for (i = 0; i < 10; ++i) {
		char s[10];
		char sql[128];

		memset(s, 'a' + i, 9);
		s[9] = 0;
		sprintf(sql, "INSERT INTO #rowset(n,c) VALUES(%d,'%s')", i+1, s);
		Command(sql);
	}

	ResetStatement();
	CHKSetStmtOption(SQL_ATTR_CURSOR_TYPE, SQL_CURSOR_DYNAMIC, "S");
	CHKExecDirect((SQLCHAR *) "SELECT * FROM #rowset ORDER BY n", SQL_NTS, "SI");

	CHKBindCol(2, SQL_C_CHAR, buf, sizeof(buf), &len, "S");

	row_count = 0xdeadbeef;
	memset(statuses, 0x55, sizeof(statuses));
	CHKExtendedFetch(SQL_FETCH_NEXT, 1, &row_count, statuses, "S");

	if (row_count != 1 || statuses[0] != SQL_ROW_SUCCESS || strcmp(buf, "aaaaaaaaa") != 0) {
		fprintf(stderr, "Invalid result\n");
		Disconnect();
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
