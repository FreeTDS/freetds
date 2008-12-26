/* Testing large objects */
/* Test from Sebastien Flaesch */

#include "common.h"

static char software_version[] = "$Id: blob1.c,v 1.15 2008-12-26 12:20:50 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define NBYTES 10000

static int failed = 0;

static void
fill_chars(char *buf, size_t len, unsigned int start, unsigned int step)
{
	size_t n;

	for (n = 0; n < len; ++n)
		buf[n] = 'a' + ((start+n) * step % ('z' - 'a' + 1));
}

static void
fill_hex(char *buf, size_t len, unsigned int start, unsigned int step)
{
	size_t n;

	for (n = 0; n < len; ++n)
		sprintf(buf + 2*n, "%2x", (unsigned int)('a' + ((start+n) * step % ('z' - 'a' + 1))));
}


static int
check_chars(const char *buf, size_t len, unsigned int start, unsigned int step)
{
	size_t n;

	for (n = 0; n < len; ++n)
		if (buf[n] != 'a' + ((start+n) * step % ('z' - 'a' + 1)))
			return 0;

	return 1;
}

static int
check_hex(const char *buf, size_t len, unsigned int start, unsigned int step)
{
	size_t n;
	char symbol[3];

	for (n = 0; n < len; ++n) {
		sprintf(symbol, "%2x", (unsigned int)('a' + ((start+n) / 2 * step % ('z' - 'a' + 1))));
		if (buf[n] != symbol[(start+n) % 2])
			return 0;
	}

	return 1;
}

static void
readBlob(SQLUSMALLINT pos)
{
	SQLRETURN rc;
	char buf[4096];
	SQLLEN len, total = 0;
	int i = 0;
	int check;

	printf(">> readBlob field %d\n", pos);
	while (1) {
		i++;
		rc = CHKGetData(pos, SQL_C_BINARY, (SQLPOINTER) buf, (SQLINTEGER) sizeof(buf), &len, "SINo");
		if (rc == SQL_NO_DATA || len <= 0)
			break;
		if (len > (SQLLEN) sizeof(buf))
			len = (SQLLEN) sizeof(buf);
		printf(">>     step %d: %d bytes readed\n", i, (int) len);
		if (pos == 1)
			check = check_chars(buf, len, 123 + total, 1);
		else
			check =	check_chars(buf, len, 987 + total, 25);
		if (!check) {
			fprintf(stderr, "Wrong buffer content\n");
			failed = 1;
		}
		total += len;
	}
	printf(">>   total bytes read = %d \n", (int) total);
	if (total != 10000)
		failed = 1;
}

static void
readBlobAsChar(SQLUSMALLINT pos, int step)
{
	SQLRETURN rc = SQL_SUCCESS_WITH_INFO;
	char buf[8192];
	SQLLEN len, total = 0;
	int i = 0;
	int check;
	int bufsize;
	
	if (step%2) bufsize = sizeof(buf) - 1;
	else bufsize = sizeof(buf);

	printf(">> readBlobAsChar field %d\n", pos);
	while (rc == SQL_SUCCESS_WITH_INFO) {
		i++;
		rc = CHKGetData(pos, SQL_C_CHAR, (SQLPOINTER) buf, (SQLINTEGER) bufsize, &len, "SINo");
		if (rc == SQL_NO_DATA || len <= 0)
			break;
		if (len > (SQLLEN) bufsize)
			len = (SQLLEN) bufsize - 1;
		len -= len % 2;
		printf(">>     step %d: %d bytes readed\n", i, (int) len);
		
		check =	check_hex(buf, len, 2*987 + total, 25);
		if (!check) {
			fprintf(stderr, "Wrong buffer content\n");
			failed = 1;
		}
		total += len;
	}
	printf(">>   total bytes read = %d \n", (int) total);
	if (total != 20000)
		failed = 1;
}


int
main(int argc, char **argv)
{
	SQLRETURN RetCode;
	SQLHSTMT old_Statement = SQL_NULL_HSTMT;
	int i;

	int key;
	SQLLEN vind0;
	char buf1[NBYTES];
	SQLLEN vind1;
	char buf2[NBYTES];
	SQLLEN vind2;
	char buf3[NBYTES*2 + 1];
	SQLLEN vind3;
	int cnt = 2;

	use_odbc_version3 = 1;
	Connect();

	Command("CREATE TABLE #tt ( k INT, t TEXT, b1 IMAGE, b2 IMAGE, v INT )");

	old_Statement = Statement;
	Statement = SQL_NULL_HSTMT;

	/* Insert rows ... */

	for (i = 0; i < cnt; i++) {

		CHKAllocHandle(SQL_HANDLE_STMT, Connection, &Statement, "S");

		CHKPrepare((SQLCHAR *) "INSERT INTO #tt VALUES ( ?, ?, ?, ?, ? )", SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &key, 0, &vind0, "S");
		CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARCHAR, 0x10000000, 0, buf1, 0, &vind1, "S");
		CHKBindParameter(3, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, 0x10000000, 0, buf2, 0, &vind2, "S");
		CHKBindParameter(4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARBINARY, 0x10000000, 0, buf3, 0, &vind3, "S");
		CHKBindParameter(5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &key, 0, &vind0, "S");

		key = i;
		vind0 = 0;

		fill_chars(buf1, NBYTES, 123, 1);
		vind1 = SQL_LEN_DATA_AT_EXEC(NBYTES);

		fill_chars(buf2, NBYTES, 987, 25);
		vind2 = SQL_LEN_DATA_AT_EXEC(NBYTES);
		
		memset(buf3, 0, sizeof(buf3));
		vind3 = SQL_LEN_DATA_AT_EXEC(2*NBYTES+1);
		

		printf(">> insert... %d\n", i);
		RetCode = CHKExecute("SINe");
		while (RetCode == SQL_NEED_DATA) {
			char *p;

			RetCode = CHKParamData((SQLPOINTER) & p, "SINe");
			printf(">> SQLParamData: ptr = %p  RetCode = %d\n", (void *) p, RetCode);
			if (RetCode == SQL_NEED_DATA) {
				if (p == buf3) {
					fill_hex(buf3, NBYTES, 987, 25);
					
					CHKPutData(p, NBYTES - (i&1), "S");

					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES - (i&1));
					
					CHKPutData(p + NBYTES - (i&1), NBYTES + (i&1), "S");

					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES + (i&1));
				} else {
					CHKPutData(p, NBYTES, "S");

					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES);
				}
			}
		}

		CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) Statement, "S");
		Statement = SQL_NULL_HSTMT;
	}

	/* Now fetch rows ... */

	for (i = 0; i < cnt; i++) {

		CHKAllocHandle(SQL_HANDLE_STMT, Connection, &Statement, "S");

		if (db_is_microsoft()) {
			CHKSetStmtAttr(SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER, "S");
			CHKSetStmtAttr(SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER, "S");
		}

		CHKPrepare((SQLCHAR *) "SELECT t, b1, b2, v FROM #tt WHERE k = ?", SQL_NTS, "S");

		CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &i, 0, &vind0, "S");

		CHKBindCol(1, SQL_C_BINARY, NULL, 0, &vind1, "S");
		CHKBindCol(2, SQL_C_BINARY, NULL, 0, &vind2, "S");
		CHKBindCol(3, SQL_C_BINARY, NULL, 0, &vind3, "S");
		CHKBindCol(4, SQL_C_LONG, &key, 0, &vind0, "S");

		vind0 = 0;
		vind1 = SQL_DATA_AT_EXEC;
		vind2 = SQL_DATA_AT_EXEC;

		CHKExecute("S");

		CHKFetchScroll(SQL_FETCH_NEXT, 0, "S");
		printf(">> fetch... %d\n", i);

		readBlob(1);
		readBlob(2);
		readBlobAsChar(3, i);

		CHKCloseCursor("S");
		CHKFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) Statement, "S");
		Statement = SQL_NULL_HSTMT;
	}
	
	Statement = old_Statement;

	Disconnect();

	return failed ? 1 : 0;
}

