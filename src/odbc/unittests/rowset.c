#include "common.h"

static char software_version[] = "$Id: rowset.c,v 1.1.2.1 2008-02-29 09:23:51 freddy77 Exp $";
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
	SQLLEN len;

	use_odbc_version3 = 1;
	Connect();

	CHK(SQLGetStmtAttr, (Statement, SQL_ROWSET_SIZE, &len, sizeof(len), NULL));
	if (len != 1) {
		fprintf(stderr, "len should be 1\n");
		Disconnect();
		return 1;
	}
	
	test_err(-123);
	test_err(-1);
	test_err(0);

	CHK(SQLSetStmtAttr, (Statement, SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(2), 0));
	CHK(SQLSetStmtAttr, (Statement, SQL_ROWSET_SIZE, (SQLPOINTER) int2ptr(1), 0));

	Disconnect();

	printf("Done.\n");
	return 0;
}
