#include "common.h"

#include <assert.h>
#include "odbcss.h"
#include <freetds/thread.h>
#include <freetds/replacements.h>

/* test query notifications */

#ifdef TDS_HAVE_MUTEX
#define SWAP(t,a,b) do { t xyz = a; a = b; b = xyz; } while(0)
#define SWAP_CONN() do { SWAP(HENV,env,odbc_env); SWAP(HDBC,dbc,odbc_conn); SWAP(HSTMT,stmt,odbc_stmt);} while(0)

static HENV env = SQL_NULL_HENV;
static HDBC dbc = SQL_NULL_HDBC;
static HSTMT stmt = SQL_NULL_HSTMT;

static TDS_THREAD_PROC_DECLARE(change_thread_proc, arg TDS_UNUSED)
{
	SQLHSTMT odbc_stmt = stmt;

	odbc_command("UPDATE ftds_test SET v = 'hi!'");
	CHKMoreResults("No");
	CHKMoreResults("SNo");

	return TDS_THREAD_RESULT(0);
}

TEST_MAIN()
{
	char *sql = NULL;
	tds_thread th;
	SQLSMALLINT cols, col;
	char message[1024];

	odbc_use_version3 = true;
	odbc_connect();

	if (!odbc_db_is_microsoft() || odbc_tds_version() < 0x702) {
		odbc_disconnect();
		printf("Query notifications available only using TDS 7.2 or newer\n");
		odbc_test_skipped();
		return 0;
	}

	sql = odbc_buf_asprintf(&odbc_buf, "ALTER DATABASE %s SET ENABLE_BROKER", common_pwd.database);
	odbc_command(sql);

	odbc_command2("DROP SERVICE FTDS_Service", "SENo");
	odbc_command2("DROP QUEUE FTDS_Queue", "SENo");
	odbc_command2("DROP TABLE ftds_test", "SENo");

	odbc_command("CREATE TABLE ftds_test(i int PRIMARY KEY, v varchar(100))");
	odbc_command("INSERT INTO ftds_test VALUES(1, 'hello')");

	odbc_command("CREATE QUEUE FTDS_Queue\n"
		     "CREATE SERVICE FTDS_Service ON QUEUE FTDS_Queue\n"
		     "([http://schemas.microsoft.com/SQL/Notifications/PostQueryNotification]);");

	/* clear queue */
	for (;;) {
		odbc_command("RECEIVE * FROM FTDS_Queue");
		if (CHKFetch("SNo") == SQL_NO_DATA)
			break;
		CHKMoreResults("SNo");
		CHKMoreResults("SNo");
	}
	odbc_reset_statement();

	/* connect another time for thread */
	SWAP_CONN();
	odbc_connect();
	SWAP_CONN();

	sql = odbc_buf_asprintf(&odbc_buf, "service=FTDS_Service;local database=%s", common_pwd.database);
	CHKSetStmtAttr(SQL_SOPT_SS_QUERYNOTIFICATION_OPTIONS, T(sql), SQL_NTS, "S");
	CHKSetStmtAttr(SQL_SOPT_SS_QUERYNOTIFICATION_MSGTEXT, T("Table has changed"), SQL_NTS, "S");
	CHKSetStmtAttr(SQL_SOPT_SS_QUERYNOTIFICATION_TIMEOUT, TDS_INT2PTR(60), SQL_IS_UINTEGER, "S");

	odbc_command("SELECT v FROM dbo.ftds_test");

	odbc_reset_statement();

	/* launch another thread to update the table we are looking to */
	assert(tds_thread_create(&th, change_thread_proc, NULL) == 0);

	odbc_command("WAITFOR (RECEIVE * FROM FTDS_Queue)");

	memset(message, 0, sizeof(message));
	CHKNumResultCols(&cols, "S");
	while (CHKFetch("SNo") == SQL_SUCCESS) {
		for (col = 0; col < cols; ++col) {
			char buf[1024];
			SQLLEN len;
			SQLTCHAR name[128];
			SQLSMALLINT namelen, type, digits, nullable;
			SQLULEN size;

			CHKDescribeCol(col + 1, name, TDS_VECTOR_SIZE(name), &namelen, &type, &size, &digits, &nullable, "S");
			if (col == 13) {
				CHKGetData(col + 1, SQL_C_BINARY, buf, sizeof(buf), &len, "S");
			} else {
				CHKGetData(col + 1, SQL_C_CHAR, buf, sizeof(buf), &len, "S");
			}
			if (col == 13) {
				int i;
				for (i = 2; i < len; i+= 2)
					buf[i / 2 - 1] = buf[i];
				buf[len / 2 - 1] = 0;
				strcpy(message, buf);
			}
			printf("%s: %s\n", C(name), buf);
		}
	}

	tds_thread_join(th, NULL);

	SWAP_CONN();
	odbc_disconnect();
	SWAP_CONN();

	/* cleanup */
	odbc_command2("DROP SERVICE FTDS_Service", "SENo");
	odbc_command2("DROP QUEUE FTDS_Queue", "SENo");
	odbc_command2("DROP TABLE ftds_test", "SENo");

	odbc_disconnect();

	assert(strstr(message, "Table has changed") != NULL);
	assert(strstr(message, "info=\"update\"") != NULL);

	return 0;
}
#else
TEST_MAIN()
{
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif
