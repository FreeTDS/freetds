#include "common.h"

/* Test for SQLPutData */

static char software_version[] = "$Id: putdata.c,v 1.12 2008-10-29 09:56:53 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static const char test_text[] =
	"Nel mezzo del cammin di nostra vita\n" "mi ritrovai per una selva oscura\n" "che' la diritta via era smarrita.";

#define BYTE_AT(n) (((n) * 245 + 123) & 0xff)

int
main(int argc, char *argv[])
{
	SQLLEN ind;
	int len = strlen(test_text), n, i;
	const char *p;
	SQLPOINTER ptr;
	unsigned char buf[256], *pb;
	SQLRETURN retcode;
	int type, lc, sql_type;

	Connect();

	/* create table to hold data */
	Command(Statement, "CREATE TABLE #putdata (c TEXT NULL, b IMAGE NULL)");

	sql_type = SQL_LONGVARCHAR;
	type = SQL_C_CHAR;
	lc = 1;
	for (;;) {
		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, type, sql_type, 0, 0, (SQLPOINTER) 123, 0, &ind));
		/* length required */
		ind = SQL_LEN_DATA_AT_EXEC(len * lc);

		/* 
		 * test for char 
		 */

		CHK(SQLPrepare, (Statement, (SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS));

		if (SQLExecute(Statement) != SQL_NEED_DATA)
			ODBC_REPORT_ERROR("Wrong result executing statement");

		p = test_text;
		n = 5;
		if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA)
			ODBC_REPORT_ERROR("Wrong result from SQLParamData");
		if (ptr != (SQLPOINTER) 123)
			ODBC_REPORT_ERROR("Wrong pointer from SQLParamData");
		while (*p) {
			int l = strlen(p);

			if (l < n)
				n = l;
			if (type == SQL_C_CHAR) {
				CHK(SQLPutData, (Statement, (char *) p, n));
			} else {
				SQLWCHAR buf[256];
				int i;
				for (i = 0; i < n; ++i)
					buf[i] = p[i];
				CHK(SQLPutData, (Statement, (char *) buf, n * lc));
			}
			p += n;
			n *= 2;
		}
		CHK(SQLParamData, (Statement, &ptr));

		if (SQLParamData(Statement, &ptr) != SQL_ERROR)
			ODBC_REPORT_ERROR("Wrong result from SQLParamData");

		/* check state  and reset some possible buffers */
		Command(Statement, "DECLARE @i INT");

		if (sql_type == SQL_LONGVARCHAR && db_is_microsoft() && db_version_int() >= 0x08000000u) {
			sql_type = SQL_WLONGVARCHAR;
			continue;
		}

		if (type != SQL_C_CHAR)
			break;
		sql_type = SQL_LONGVARCHAR;
		type = SQL_C_WCHAR;
		lc = sizeof(SQLWCHAR);
	}

	/* update row setting binary field */
	for (i = 0; i < 255; ++i)
		buf[i] = BYTE_AT(i);

	/* 
	 * test for binary 
	 */

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, 0, 0, (SQLPOINTER) 4567, 0, &ind));
	ind = SQL_LEN_DATA_AT_EXEC(254);

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "UPDATE #putdata SET b = ?", SQL_NTS));

	if (SQLExecute(Statement) != SQL_NEED_DATA)
		ODBC_REPORT_ERROR("Wrong result executing statement");

	pb = buf;
	n = 7;
	if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA)
		ODBC_REPORT_ERROR("Wrong result from SQLParamData");
	if (ptr != (SQLPOINTER) 4567)
		ODBC_REPORT_ERROR("Wrong pointer from SQLParamData");
	while (pb != (buf + 254)) {
		int l = buf + 254 - pb;

		if (l < n)
			n = l;
		CHK(SQLPutData, (Statement, (char *) p, n));
		pb += n;
		n *= 2;
	}
	CHK(SQLParamData, (Statement, &ptr));

	if (SQLParamData(Statement, &ptr) != SQL_ERROR)
		ODBC_REPORT_ERROR("Wrong result from SQLParamData");

	/* check state  and reset some possible buffers */
	Command(Statement, "DECLARE @i2 INT");


	/* test len == 0 case from ML */
	CHK(SQLFreeStmt, (Statement, SQL_RESET_PARAMS));

	type = SQL_C_CHAR;
	for (;;) {
		CHK(SQLPrepare, (Statement, (SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS));

		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, type, SQL_LONGVARCHAR, 0, 0, (PTR) 2, 0, &ind));

		ind = SQL_LEN_DATA_AT_EXEC(0);

		if ((retcode = SQLExecute(Statement)) != SQL_NEED_DATA) {
			printf("Wrong result executing statement (retcode=%d)\n", (int) retcode);
			exit(1);
		}
		while (retcode == SQL_NEED_DATA) {
			retcode = SQLParamData(Statement, &ptr);
			if (retcode == SQL_NEED_DATA) {
				SQLPutData(Statement, "abc", 3);
			}
		}
		if (type != SQL_C_CHAR)
			break;
		type = SQL_C_WCHAR;
		ResetStatement();
	}

	/* TODO check inserts ... */
	/* TODO test cancel inside SQLExecute */

	Disconnect();

	printf("Done.\n");
	return 0;
}
