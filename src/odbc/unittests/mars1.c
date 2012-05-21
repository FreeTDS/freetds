#include "common.h"

/* first MARS test, test 2 concurrent recordset */

#define SWAP_STMT(b) do { SQLHSTMT xyz = odbc_stmt; odbc_stmt = b; b = xyz; } while(0)

static void
my_attrs(void)
{
	SQLSetConnectAttr(odbc_conn, 1224 /*SQL_COPT_SS_MARS_ENABLED*/, (SQLPOINTER) 1 /*SQL_MARS_ENABLED_YES*/, SQL_IS_UINTEGER);
}

int
main(int argc, char *argv[])
{
	SQLINTEGER len, out;
	int i;
	SQLHSTMT stmt2;

	odbc_use_version3 = 1;
	odbc_set_conn_attr = my_attrs;
	odbc_connect();

	out = 0;
	len = sizeof(out);
	CHKGetConnectAttr(1224, (SQLPOINTER) &out, sizeof(out), &len, "S");

	/* test we really support MARS on this connection */
	/* TODO should out be correct ?? */
	printf("Following row can contain an error due to MARS detection (is expected)\n");
	if (!out || odbc_command2("BEGIN TRANSACTION", "SNoE") != SQL_ERROR) {
		printf("MARS not supported for this connection\n");
		odbc_disconnect();
		return 0;
	}
	odbc_read_error();
	if (!strstr(odbc_err, "MARS")) {
		printf("Error message invalid \"%s\"\n", odbc_err);
		return 1;
	}

	/* create a test table with some data */
	odbc_command("create table #mars1 (n int, v varchar(100))");
	for (i = 0; i < 60; ++i) {
		char cmd[120], buf[80];
		memset(buf, 'a' + (i % 26), sizeof(buf));
		buf[i * 7 % 73] = 0;
		sprintf(cmd, "insert into #mars1 values(%d, '%s')", i, buf);
		odbc_command(cmd);
	}

	/* and another to avid locking problems */
	odbc_command("create table #mars2 (n int, v varchar(100))");

	/* try to do a select which return a lot of data (to test server didn't cache everything) */
	odbc_command("select a.n, b.n, a.v from #mars1 a, #mars1 b order by a.n, b.n");
	CHKFetch("S");

	/* try to do other commands */
	CHKAllocStmt(&stmt2, "S");
	SWAP_STMT(stmt2);
	odbc_command("insert into #mars2 values(1, 'foo')");
	SWAP_STMT(stmt2);

	CHKFetch("S");

	/* TODO test receiving large data should not take much memory */

	odbc_disconnect();
	return 0;
}

