#include "common.h"

/* test error on prepared statement, from Nathaniel Talbott test */

static char software_version[] = "$Id: preperror.c,v 1.4.2.1 2005-08-14 09:00:45 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
ResetStatement(void)
{
	SQLFreeStmt(Statement, SQL_DROP);
	Statement = SQL_NULL_HSTMT;
	if (SQLAllocStmt(Connection, &Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to allocate statement");
}

int
main(int argc, char *argv[])
{
	SQLLEN cbInString = SQL_NTS;
	char buf[256];
	SQLRETURN ret;
	unsigned char sqlstate[6];

	Connect();

	Command(Statement, "CREATE TABLE #urls ( recdate DATETIME ) ");

	/* test implicit conversion error */
	if (CommandWithResult(Statement, "INSERT INTO #urls ( recdate ) VALUES ( '2003-10-1 10:11:1 0' )") != SQL_ERROR) {
		fprintf(stderr, "SQLExecDirect success instead of failing!\n");
		return 1;
	}

	/* test prepared implicit conversion error */
	if (!SQL_SUCCEEDED(SQLPrepare(Statement, (SQLCHAR *) "INSERT INTO #urls ( recdate ) VALUES ( ? )", SQL_NTS))) {
		fprintf(stderr, "SQLPrepare failure!\n");
		return 1;
	}

	strcpy(buf, "2003-10-1 10:11:1 0");
	if (!SQL_SUCCEEDED
	    (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 128, 0, buf, sizeof(buf), &cbInString))) {
		fprintf(stderr, "SQLBindParameter failure!\n");
		return 1;
	}

	if (SQLExecute(Statement) != SQL_ERROR) {
		fprintf(stderr, "SQLExecute succeeded instead of failing! (line %d)\n", __LINE__);
		return 1;
	}

	ret = SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *) buf, sizeof(buf), NULL);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Error not set (line %d)\n", __LINE__);
		return 1;
	}
	printf("err=%s\n", buf);

	/* assure initial state */
	ResetStatement();

	/* try to prepare and execute a statement with error (from DBD::ODBC test) */
	ret = SQLPrepare(Statement, (SQLCHAR *) "SELECT XXNOTCOLUMN FROM sysobjects", SQL_NTS);

	if (ret == SQL_SUCCESS)
		ret = SQLExecute(Statement);

	if (ret != SQL_ERROR) {
		fprintf(stderr, "SQLExecute succeeded instead of failing! (line %d)\n", __LINE__);
		return 1;
	}

	ret = SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *) buf, sizeof(buf), NULL);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Error not set (line %d)\n", __LINE__);
		return 1;
	}
	printf("err=%s\n", buf);


	Disconnect();

	printf("Done.\n");
	return 0;
}
