/* Testing large objects */
/* Test from Sebastien Flaesch */

#include "common.h"

static char software_version[] = "$Id: blob1.c,v 1.5 2008-07-05 22:58:01 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHECK_RCODE(t,h,m) \
   if ( rcode != SQL_SUCCESS && rcode != SQL_SUCCESS_WITH_INFO && rcode != SQL_NO_DATA && rcode != SQL_NEED_DATA ) { \
      fprintf(stderr,"Error %d at: %s\n",rcode,m); \
      getErrorInfo(t,h); \
      exit(1); \
   }

#define NBYTES 10000

static int failed = 0;

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLRETURN rcode = 0;
	SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
	SQLINTEGER naterror = 0;
	SQLCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	rcode = SQLGetDiagRec((SQLSMALLINT) sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      (SQLSMALLINT) 1,
			      (SQLCHAR *) sqlstate,
			      (SQLINTEGER *) & naterror,
			      (SQLCHAR *) msgtext, (SQLSMALLINT) sizeof(msgtext), (SQLSMALLINT *) & msgtextl);
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", (char *) sqlstate);
	fprintf(stderr, "  SQL code : %d\n", (int) naterror);
	fprintf(stderr, "  Message  : %s\n", (char *) msgtext);
}

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

static int
readBlob(SQLHSTMT * stmth, SQLUSMALLINT pos)
{
	SQLRETURN rcode;
	char buf[4096];
	SQLLEN len, total = 0;
	int i = 0;
	int check;

	printf(">> readBlob field %d\n", pos);
	while (1) {
		i++;
		rcode = SQLGetData(stmth, pos, SQL_C_BINARY, (SQLPOINTER) buf, (SQLINTEGER) sizeof(buf), &len);
		if (!SQL_SUCCEEDED(rcode) || len <= 0)
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
	return rcode;
}

static int
readBlobAsChar(SQLHSTMT * stmth, SQLUSMALLINT pos, int step)
{
	SQLRETURN rcode = SQL_SUCCESS_WITH_INFO;
	char buf[8192];
	SQLLEN len, total = 0;
	int i = 0;
	int check;
	int bufsize;
	
	if (step%2) bufsize = sizeof(buf) - 1;
	else bufsize = sizeof(buf);

	printf(">> readBlobAsChar field %d\n", pos);
	while (rcode == SQL_SUCCESS_WITH_INFO) {
		i++;
		rcode = SQLGetData(stmth, pos, SQL_C_CHAR, (SQLPOINTER) buf, (SQLINTEGER) bufsize, &len);
		if (!SQL_SUCCEEDED(rcode) || len <= 0)
			break;
		if (len > (SQLLEN) bufsize)
			len = (SQLLEN) bufsize - 1;
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
	return rcode;
}


int
main(int argc, char **argv)
{
	SQLRETURN rcode;
	SQLHSTMT m_hstmt = NULL;
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

	Command(Statement, "CREATE TABLE #tt ( k INT, t TEXT, b1 IMAGE, b2 IMAGE, v INT )");

	/* Insert rows ... */

	for (i = 0; i < cnt; i++) {

		m_hstmt = NULL;
		rcode = SQLAllocHandle(SQL_HANDLE_STMT, Connection, &m_hstmt);
		CHECK_RCODE(SQL_HANDLE_DBC, Connection, "SQLAllocHandle StmtH");

		rcode = SQLPrepare(m_hstmt, (SQLCHAR *) "INSERT INTO #tt VALUES ( ?, ?, ?, ?, ? )", SQL_NTS);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLPrepare");

		SQLBindParameter(m_hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &key, 0, &vind0);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 1");

		SQLBindParameter(m_hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARCHAR, 0x10000000, 0, buf1, 0, &vind1);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 2");

		SQLBindParameter(m_hstmt, 3, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, 0x10000000, 0, buf2, 0, &vind2);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 3");

		SQLBindParameter(m_hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARBINARY, 0x10000000, 0, buf3, 0, &vind3);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 4");

		SQLBindParameter(m_hstmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &key, 0, &vind0);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 5");

		key = i;
		vind0 = 0;

		fill_chars(buf1, NBYTES, 123, 1);
		vind1 = SQL_LEN_DATA_AT_EXEC(NBYTES);

		fill_chars(buf2, NBYTES, 987, 25);
		vind2 = SQL_LEN_DATA_AT_EXEC(NBYTES);
		
		memset(buf3, 0, sizeof(buf3));
		vind3 = SQL_LEN_DATA_AT_EXEC(2*NBYTES+1);
		

		printf(">> insert... %d\n", i);
		rcode = SQLExecute(m_hstmt);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLExecute StmtH");
		while (rcode == SQL_NEED_DATA) {
			char *p;

			rcode = SQLParamData(m_hstmt, (SQLPOINTER) & p);
			CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLParamData StmtH");
			printf(">> SQLParamData: ptr = %p  rcode = %d\n", (void *) p, rcode);
			if (rcode == SQL_NEED_DATA) {
				SQLRETURN rcode;
				if (p == buf3) {
					fill_hex(buf3, NBYTES, 987, 25);
					
					rcode = SQLPutData(m_hstmt, p, NBYTES - (i&1));

					CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLPutData StmtH");
					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES - (i&1));
					
					rcode = SQLPutData(m_hstmt, p + NBYTES - (i&1), NBYTES + (i&1));

					CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLPutData StmtH");
					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES + (i&1));
				} else {
					rcode = SQLPutData(m_hstmt, p, NBYTES);

					CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLPutData StmtH");
					printf(">> param %p: total bytes written = %d\n", (void *) p, NBYTES);
				}
			}
		}

		rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLFreeHandle StmtH");

	}

	/* Now fetch rows ... */

	for (i = 0; i < cnt; i++) {

		m_hstmt = NULL;
		rcode = SQLAllocHandle(SQL_HANDLE_STMT, Connection, &m_hstmt);
		CHECK_RCODE(SQL_HANDLE_DBC, Connection, "SQLAllocHandle StmtH");

		if (db_is_microsoft()) {
			rcode = SQLSetStmtAttr(m_hstmt, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER) SQL_NONSCROLLABLE, SQL_IS_UINTEGER);
			CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLSetStmtAttr SQL_ATTR_CURSOR_SCROLLABLE");
			rcode = SQLSetStmtAttr(m_hstmt, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER) SQL_SENSITIVE, SQL_IS_UINTEGER);
			CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLSetStmtAttr SQL_ATTR_CURSOR_SENSITIVITY");
		}

		rcode = SQLPrepare(m_hstmt, (SQLCHAR *) "SELECT t, b1, b2, v FROM #tt WHERE k = ?", SQL_NTS);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLPrepare");

		SQLBindParameter(m_hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &i, 0, &vind0);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindParameter 1");

		SQLBindCol(m_hstmt, 1, SQL_C_BINARY, NULL, 0, &vind1);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindCol 2");
		SQLBindCol(m_hstmt, 2, SQL_C_BINARY, NULL, 0, &vind2);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindCol 3");
		SQLBindCol(m_hstmt, 3, SQL_C_BINARY, NULL, 0, &vind3);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindCol 4");
		SQLBindCol(m_hstmt, 4, SQL_C_LONG, &key, 0, &vind0);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLBindCol 1");

		vind0 = 0;
		vind1 = SQL_DATA_AT_EXEC;
		vind2 = SQL_DATA_AT_EXEC;

		rcode = SQLExecute(m_hstmt);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLExecute StmtH");

		rcode = SQLFetchScroll(m_hstmt, SQL_FETCH_NEXT, 0);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLFetchScroll StmtH");
		printf(">> fetch... %d  rcode = %d\n", i, rcode);

		rcode = readBlob(m_hstmt, 1);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "readBlob 1");
		rcode = readBlob(m_hstmt, 2);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "readBlob 2");
		rcode = readBlobAsChar(m_hstmt, 3, i);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "readBlob 3 as SQL_C_CHAR");

		rcode = SQLCloseCursor(m_hstmt);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLCloseCursor StmtH");

		rcode = SQLFreeHandle(SQL_HANDLE_STMT, (SQLHANDLE) m_hstmt);
		CHECK_RCODE(SQL_HANDLE_STMT, m_hstmt, "SQLFreeHandle StmtH");
	}

	Disconnect();

	return failed ? 1 : 0;
}

