#include "common.h"

/* Test for SQLPutData */

static char software_version[] = "$Id: putdata.c,v 1.13 2008-11-04 10:59:02 freddy77 Exp $";
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
	SQLRETURN RetCode;
	int type, lc, sql_type;

	Connect();

	/* create table to hold data */
	Command(Statement, "CREATE TABLE #putdata (c TEXT NULL, b IMAGE NULL)");

	sql_type = SQL_LONGVARCHAR;
	type = SQL_C_CHAR;
	lc = 1;
	for (;;) {
		CHKBindParameter(1, SQL_PARAM_INPUT, type, sql_type, 0, 0, (SQLPOINTER) 123, 0, &ind, "S");
		/* length required */
		ind = SQL_LEN_DATA_AT_EXEC(len * lc);

		/* 
		 * test for char 
		 */

		CHKPrepare((SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS, "S");

		CHKExecute("Ne");

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
				CHKPutData((char *) p, n, "S");
			} else {
				SQLWCHAR buf[256];
				int i;
				for (i = 0; i < n; ++i)
					buf[i] = p[i];
				CHKPutData((char *) buf, n * lc, "S");
			}
			p += n;
			n *= 2;
		}
		CHKParamData(&ptr, "S");

		CHKParamData(&ptr, "E");

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

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, 0, 0, (SQLPOINTER) 4567, 0, &ind, "S");
	ind = SQL_LEN_DATA_AT_EXEC(254);

	CHKPrepare((SQLCHAR *) "UPDATE #putdata SET b = ?", SQL_NTS, "S");

	CHKExecute("Ne");

	pb = buf;
	n = 7;
	CHKParamData(&ptr, "Ne");
	if (ptr != (SQLPOINTER) 4567)
		ODBC_REPORT_ERROR("Wrong pointer from SQLParamData");
	while (pb != (buf + 254)) {
		int l = buf + 254 - pb;

		if (l < n)
			n = l;
		CHKPutData((char *) p, n, "S");
		pb += n;
		n *= 2;
	}
	CHKParamData(&ptr, "S");

	CHKParamData(&ptr, "E");

	/* check state  and reset some possible buffers */
	Command(Statement, "DECLARE @i2 INT");


	/* test len == 0 case from ML */
	CHKFreeStmt(SQL_RESET_PARAMS, "S");

	type = SQL_C_CHAR;
	for (;;) {
		CHKPrepare((SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, type, SQL_LONGVARCHAR, 0, 0, (PTR) 2, 0, &ind, "S");

		ind = SQL_LEN_DATA_AT_EXEC(0);

		CHKExecute("Ne");
		while (RetCode == SQL_NEED_DATA) {
			RetCode = SQLParamData(Statement, &ptr);
			if (RetCode == SQL_NEED_DATA) {
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
