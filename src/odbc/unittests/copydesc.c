#include "common.h"

/* Test SQLCopyDesc and SQLAllocHandle(SQL_HANDLE_DESC) */

static void
check_alloc_type(SQLHDESC hdesc, SQLSMALLINT expected_alloc_type, const char *exp, int line)
{
	SQLSMALLINT alloc_type;
	SQLINTEGER ind;

	CHKGetDescField(hdesc, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, sizeof(alloc_type), &ind, "S");

	if (alloc_type != expected_alloc_type) {
		fprintf(stderr, "Wrong condition at line %d: SQL_DESC_ALLOC_TYPE expected %s(%d) got %d\n",
			line, exp, expected_alloc_type, alloc_type);
		exit(1);
	}
}

#define check_alloc_type(d,e) check_alloc_type(d, e, #e, __LINE__)

TEST_MAIN()
{
	SQLHDESC ard, ard2, ard3;
	SQLINTEGER id;
	SQLLEN ind1, ind2;
	char name[64];

	odbc_connect();

	CHKGetStmtAttr(SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL, "S");
	check_alloc_type(ard, SQL_DESC_ALLOC_AUTO);

	CHKBindCol(1, SQL_C_SLONG, &id, sizeof(SQLINTEGER), &ind1, "S");
	CHKBindCol(2, SQL_C_CHAR, name, sizeof(name), &ind2, "S");

	CHKAllocHandle(SQL_HANDLE_DESC, odbc_conn, &ard2, "S");
	check_alloc_type(ard2, SQL_DESC_ALLOC_USER);

	/*
	 * this is an additional test to test additional allocation 
	 * As of 0.64 for a bug in SQLAllocDesc we only allow to allocate one
	 */
	CHKAllocHandle(SQL_HANDLE_DESC, odbc_conn, &ard3, "S");
	check_alloc_type(ard3, SQL_DESC_ALLOC_USER);

	CHKR(SQLCopyDesc, (ard, ard2), "S");
	check_alloc_type(ard2, SQL_DESC_ALLOC_USER);

	CHKFreeHandle(SQL_HANDLE_DESC, ard3, "S");

	/* check SQL_INVALID_HANDLE, twice to check a mutex condition */
	CHKR(SQLCopyDesc, (NULL, ard2), "V");
	CHKR(SQLCopyDesc, (ard2, NULL), "V");
	CHKR(SQLCopyDesc, (NULL, ard2), "V");
	CHKR(SQLCopyDesc, (ard2, NULL), "V");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
