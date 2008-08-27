#include "common.h"

static char software_version[] = "$Id: rowset.c,v 1.1.2.3 2008-08-27 08:16:03 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHK(func,params) \
	if (func params != SQL_SUCCESS) \
		ODBC_REPORT_ERROR(#func)

static char odbc_err[256];
static char odbc_sqlstate[6];

static void
ReadError(void)
{
	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, (SQLCHAR *) odbc_sqlstate, NULL, (SQLCHAR *) odbc_err, sizeof(odbc_err), NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

static void
test_err(int n)
{
	SQLRETURN rc;

	rc = SQLSetStmtAttr(Statement, SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(n), 0);
	if (rc != SQL_ERROR) {
		fprintf(stderr, "SQLSetStmtAttr should fail\n");
		Disconnect();
		exit(1);
        }
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
	SQLRETURN rc;

	use_odbc_version3 = 1;
	Connect();

	/* initial value should be 1 */
	CHK(SQLGetStmtAttr, (Statement, SQL_ROWSET_SIZE, &len, sizeof(len), NULL));
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
	CHK(SQLSetStmtAttr, (Statement, SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(2), 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(1), 0));

	/* now check that SQLExtendedFetch works as expected */
	Command(Statement, "CREATE TABLE #rowset(n INTEGER, c VARCHAR(20))");
	for (i = 0; i < 10; ++i) {
		char s[10];
		char sql[128];

		memset(s, 'a' + i, 9);
		s[9] = 0;
		sprintf(sql, "INSERT INTO #rowset(n,c) VALUES(%d,'%s')", i+1, s);
		Command(Statement, sql);
	}

	ResetStatement();
	CHK(SQLSetStmtOption, (Statement, SQL_ATTR_CURSOR_TYPE, SQL_CURSOR_DYNAMIC));
	rc = CommandWithResult(Statement, "SELECT * FROM #rowset ORDER BY n");
	if (!SQL_SUCCEEDED(rc))
		ODBC_REPORT_ERROR("SQLExecDirect error");

	CHK(SQLBindCol, (Statement, 2, SQL_C_CHAR, buf, sizeof(buf), &len));

	row_count = 0xdeadbeef;
	memset(statuses, 0x55, sizeof(statuses));
	CHK(SQLExtendedFetch, (Statement, SQL_FETCH_NEXT, 1, &row_count, statuses));

	if (row_count != 1 || statuses[0] != SQL_ROW_SUCCESS || strcmp(buf, "aaaaaaaaa") != 0) {
		fprintf(stderr, "Invalid result\n");
		Disconnect();
		return 1;
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
