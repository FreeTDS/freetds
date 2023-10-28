#include "common.h"

/* Test for SQLPutData */

static const char test_text[] =
	"Nel mezzo del cammin di nostra vita\n" "mi ritrovai per una selva oscura\n" "che' la diritta via era smarrita.";

#define BYTE_AT(n) (((n) * 245 + 123) & 0xff)

static int
to_sqlwchar(SQLWCHAR *dst, const char *src, int n)
{
	int i;
	for (i = 0; i < n; ++i)
		dst[i] = src[i];
	return n * sizeof(SQLWCHAR);
}

static char sql[1024];

static void
Test(int direct)
{
	SQLLEN ind;
	int len = strlen(test_text), n, i;
	const char *p;
	char *pp;
	SQLPOINTER ptr;
	unsigned char buf[256], *pb;
	SQLRETURN RetCode;
	int type, lc, sql_type;

	/* create table to hold data */
	odbc_command_with_result(odbc_stmt, "DROP TABLE #putdata");
	odbc_command("CREATE TABLE #putdata (c TEXT NULL, b IMAGE NULL)");

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

		if (!direct) {
			CHKPrepare(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "S");

			CHKExecute("Ne");
		} else {
			CHKExecDirect(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "Ne");
		}

		p = test_text;
		n = 5;
		ptr = ((char*)0) + 0xdeadbeef;
		CHKParamData(&ptr, "Ne");
		if (ptr != (SQLPOINTER) 123) {
			fprintf(stderr, "%p\n", ptr);
			ODBC_REPORT_ERROR("Wrong pointer from SQLParamData");
		}
		while (*p) {
			int l = strlen(p);

			if (l < n)
				n = l;
			if (type == SQL_C_CHAR) {
				CHKPutData((char *) p, n, "S");
			} else {
				SQLWCHAR buf[256];
				CHKPutData((char *) buf, to_sqlwchar(buf, p, n), "S");
			}
			p += n;
			n *= 2;
		}
		CHKParamData(&ptr, "S");

		CHKParamData(&ptr, "E");

		/* check state  and reset some possible buffers */
		odbc_command("DECLARE @i INT");

		/* use server ntext if available */
		if (sql_type == SQL_LONGVARCHAR && odbc_db_is_microsoft() && odbc_db_version_int() >= 0x08000000u) {
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

	CHKPrepare(T("UPDATE #putdata SET b = ?"), SQL_NTS, "S");

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
		CHKPutData((char *) pb, n, "S");
		pb += n;
		n *= 2;
	}
	CHKParamData(&ptr, "S");

	CHKParamData(&ptr, "E");

	/* check state  and reset some possible buffers */
	odbc_command("DECLARE @i2 INT");


	CHKFreeStmt(SQL_RESET_PARAMS, "S");

	/* check inserts ... there should be all equal rows */
	strcpy(sql, "IF EXISTS(SELECT * FROM #putdata WHERE CONVERT(VARBINARY(255),b) <> 0x");
	/* append binary */
	for (i = 0; i < 254; ++i)
		sprintf(strchr(sql, 0), "%02x", buf[i]);
	strcat(sql, " OR CONVERT(VARCHAR(255),c) <> '");
	/* append string */
	pp = strchr(sql, 0);
	p = test_text;
	do {
		*pp++ = *p;
		if (*p == '\'')
			*pp++ = *p;
	} while(*p++);
	strcat(sql, "') SELECT 1");
	odbc_check_no_row(sql);

	odbc_command("DELETE FROM #putdata");

	/* test len == 0 case from ML */
	type = SQL_C_CHAR;
	for (;;) {
		if (!direct)
			CHKPrepare(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, type, SQL_LONGVARCHAR, 0, 0, (PTR) 2, 0, &ind, "S");

		ind = SQL_LEN_DATA_AT_EXEC(0);

		if (!direct)
			CHKExecute("Ne");
		else
			CHKExecDirect(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "Ne");
		RetCode = SQL_NEED_DATA;
		while (RetCode == SQL_NEED_DATA) {
			RetCode = SQLParamData(odbc_stmt, &ptr);
			if (RetCode == SQL_NEED_DATA) {
				if (type == SQL_C_CHAR) {
					CHKPutData("abc", 3, "S");
				} else {
					SQLWCHAR buf[10];
					CHKPutData(buf, to_sqlwchar(buf, "abc", 3), "S");
				}
			}
		}
		if (type != SQL_C_CHAR)
			break;
		type = SQL_C_WCHAR;
		odbc_reset_statement();
	}

	/* check inserts ... */
	odbc_check_no_row("IF EXISTS(SELECT * FROM #putdata WHERE c NOT LIKE 'abc') SELECT 1");

	odbc_command("DELETE FROM #putdata");

	/* test putting 0 bytes from Sebastien Flaesch */
	if (odbc_db_is_microsoft()) {
		type = SQL_C_CHAR;
		for (;;) {
			if (!direct)
				CHKPrepare(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "S");

			CHKBindParameter(1, SQL_PARAM_INPUT, type, SQL_LONGVARCHAR, 10, 0, (PTR) 2, 10, &ind, "S");

			ind = SQL_DATA_AT_EXEC;

			if (!direct)
				CHKExecute("Ne");
			else
				CHKExecDirect(T("INSERT INTO #putdata(c) VALUES(?)"), SQL_NTS, "Ne");
			RetCode = SQL_NEED_DATA;
			while (RetCode == SQL_NEED_DATA) {
				RetCode = SQLParamData(odbc_stmt, &ptr);
				if (RetCode == SQL_NEED_DATA)
					CHKPutData("", 0, "S");
			}
			if (type != SQL_C_CHAR)
				break;
			type = SQL_C_WCHAR;
			odbc_reset_statement();
		}

		/* check inserts ... */
		odbc_check_no_row("IF EXISTS(SELECT * FROM #putdata WHERE c NOT LIKE '') SELECT 1");
	}

	/* TODO test cancel inside SQLExecute */
}

int
main(void)
{
	odbc_connect();

	Test(0);
	Test(1);

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
