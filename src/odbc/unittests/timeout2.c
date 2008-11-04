#include "common.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <assert.h>

/*
 * Test timeout on prepare
 * It execute a query wait for timeout and then try to issue a new prepare/execute
 * This test a BUG where second prepare timeouts
 *
 * Test from Ou Liu, cf "Query Time Out", 2006-08-08
 */

static char software_version[] = "$Id: timeout2.c,v 1.6 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#if defined(__MINGW32__) || defined(WIN32)
#define sleep(s) Sleep((s)*1000)
#endif

int
main(int argc, char *argv[])
{
	int i;

	Connect();

	Command(Statement, "create table #timeout(i int)");
	Command(Statement, "insert into #timeout values(1)");

	for (i = 0; i < 2; ++i) {

		printf("Loop %d\n", i);

		CHKSetStmtAttr(SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 10, SQL_IS_UINTEGER, "S");

		CHKPrepare((SQLCHAR*) "select * from #timeout", SQL_NTS, "S");
		CHKExecute("S");

		do {
			while (CHKFetch("SNo") == SQL_SUCCESS)
				;
		} while (CHKMoreResults("SNo") == SQL_SUCCESS);

		if (i == 0) {
			printf("Sleep 15 seconds to test if timeout occurs\n");
			sleep(15);
		}

		SQLFreeStmt(Statement, SQL_CLOSE);
		SQLFreeStmt(Statement, SQL_UNBIND);
		SQLFreeStmt(Statement, SQL_RESET_PARAMS);
		SQLCloseCursor(Statement);
	}

	Disconnect();

	return 0;
}
