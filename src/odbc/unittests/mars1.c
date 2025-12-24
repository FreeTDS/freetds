#include "common.h"

/* first MARS test, test 2 concurrent recordset */

#define SET_STMT(n) do { \
	if (pcur_stmt != &n) { \
		if (pcur_stmt) *pcur_stmt = odbc_stmt; \
		pcur_stmt = &n; \
		odbc_stmt = *pcur_stmt; \
	} \
} while(0)

static void
AutoCommit(int onoff)
{
	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, TDS_INT2PTR(onoff), 0, "S");
}

static void
EndTransaction(SQLSMALLINT type)
{
	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, type, "S");
}


static void
my_attrs(void)
{
	SQLSetConnectAttr(odbc_conn, 1224 /*SQL_COPT_SS_MARS_ENABLED*/, (SQLPOINTER) 1 /*SQL_MARS_ENABLED_YES*/, SQL_IS_UINTEGER);
}

TEST_MAIN()
{
	SQLINTEGER len, out;
	int i, j;
	SQLHSTMT stmt1, stmt2;
	SQLHSTMT *pcur_stmt = NULL;

	odbc_use_version3 = true;
	odbc_set_conn_attr = my_attrs;
	odbc_connect();

	stmt1 = odbc_stmt;

	out = 0;
	len = sizeof(out);
	CHKGetConnectAttr(1224, (SQLPOINTER) &out, sizeof(out), &len, "SE");

	/* test we really support MARS on this connection */
	/* TODO should out be correct ?? */
	printf("Following row can contain an error due to MARS detection (is expected)\n");
	if (!out || odbc_command2("BEGIN TRANSACTION", "SNoE") != SQL_ERROR) {
		printf("MARS not supported for this connection\n");
		odbc_disconnect();
		odbc_test_skipped();
		return 0;
	}
	odbc_read_error();
	if (!strstr(odbc_err, "MARS")) {
		fprintf(stderr, "Error message invalid \"%s\"\n", odbc_err);
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

	AutoCommit(SQL_AUTOCOMMIT_OFF);

	/* try to do a select which return a lot of data (to test server didn't cache everything) */
	odbc_command("select a.n, b.n, c.n, a.v from #mars1 a, #mars1 b, #mars1 c order by a.n, b.n, c.n");
	CHKFetch("S");

	CHKAllocStmt(&stmt2, "S");

	/* Use a parameterized insert. This causes DONEINPROC to be returned by SQL Server,
	 * leading to result_type==TDS_CMD_DONE when it's complete. Without the parameter,
	 * result_type == TDS_DONE_RESULT.
	 * And in odbc_SQLExecute(), it calls odbc_unlock_statement() for TDS_CMD_DONE, but
	 * not for TDS_DONE_RESULT. (We don't know why...)
	 * This means that the stmt->tds TDSSOCKET struct is completely freed after every
	 * iteration of the insert if and only if it was a parameterized insert. So we need
	 * to test both parameterized and non-parameterized inserts.
	 */
	long bind1;
	char bind2[20] = "parameters";
	SQLBindParameter(stmt2, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &bind1, 0, NULL);
	SQLBindParameter(stmt2, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 0, 0, &bind2, 20, NULL);

	/* adjust these parameters for memory leak testing */
	/* TODO a way for this test to detect memory leak here. */
	const int n_iterations = 20;		/* E.g. 200000 */
	const int freq_parameterized = 2;	/* set 1 to parameterize all, INT_MAX for none */

	for (i= 1; i <= n_iterations; ++i)
	{
		SET_STMT(stmt2);

		// Test option - force reallocation of socket
		// odbc_reset_statement();

		if (i % freq_parameterized == 0)
		{
			bind1 = i;
			odbc_command("!insert into #mars2 values(?, ?)");
		}
		else
			odbc_command("!insert into #mars2 values(1, 'foo')");

		// Perform several fetches for each insert, so we also test continuing to draw
		// further packets of the fetch
		SET_STMT(stmt1);
		for (j = 0; j < 10; ++j)
		{
			CHKFetch("S");
		}
	}
	printf("Performed %d inserts while fetching.\n", i - 1);
	
	/* reset statements */
	SET_STMT(stmt1);
	odbc_reset_statement();
	SET_STMT(stmt2);
	odbc_reset_statement();
	goto done;
	/* now to 2 select with prepare/execute */
	CHKPrepare(T("select a.n, b.n, a.v from #mars1 a, #mars1 b order by a.n, b.n"), SQL_NTS, "S");
	SET_STMT(stmt1);
	CHKPrepare(T("select a.n, b.n, a.v from #mars1 a, #mars1 b order by a.n desc, b.n"), SQL_NTS, "S");
	SET_STMT(stmt2);
	CHKExecute("S");
	SET_STMT(stmt1);
	CHKExecute("S");
	SET_STMT(stmt2);
	CHKFetch("S");
	SET_STMT(stmt1);
	CHKFetch("S");
	SET_STMT(stmt2);
	CHKFetch("S");
	odbc_reset_statement();
	SET_STMT(stmt1);
	CHKFetch("S");
	odbc_reset_statement();
	done:
	EndTransaction(SQL_COMMIT);

	odbc_disconnect();
	return 0;
}

