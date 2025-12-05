#include "common.h"

#include <assert.h>
#include <freetds/utils.h>

/* change password on server */

#define USER "freetds_oldpwd"
#define PWD1 "TestPWD1?"
#define PWD2 "OtherPWDButLonger2@"

static void
my_attrs(void)
{
	SQLSetConnectAttr(odbc_conn, 1226 /* SQL_COPT_SS_OLDPWD */ ,
			  (SQLPOINTER) T(PWD1), SQL_NTS);
	strcpy(common_pwd.user, USER);
	strcpy(common_pwd.password, PWD2);
}

static HENV save_env;
static HDBC save_conn;
static HSTMT save_stmt;

static void
swap_conn(void)
{
	ODBC_SWAP(HENV, odbc_env, save_env);
	ODBC_SWAP(HDBC, odbc_conn, save_conn);
	ODBC_SWAP(HSTMT, odbc_stmt, save_stmt);
}

TEST_MAIN()
{
	const char *ci;
	SQLRETURN rc;

	odbc_use_version3 = true;

	/*
	 * Check if we are in CI.
	 * This test is doing some administration setting on the server, avoid
	 * to do on all machines, especially if users are trying to run tests
	 * without knowing this.
	 */
	ci = getenv("CI");
	if (!ci || strcasecmp(ci, "true") != 0) {
		odbc_test_skipped();
		return 0;
	}

	odbc_connect();

	/* minimum TDS 7.2 and MSSQL 2012 */
	if (odbc_tds_version() < 0x702 || odbc_db_version_int() < 0x0a000000u) {
		odbc_disconnect();
		odbc_test_skipped();
		return 0;
	}

	/* create new login for this test and disconnect */
	odbc_command_with_result(odbc_stmt, "DROP LOGIN " USER);
	rc = odbc_command2("CREATE LOGIN " USER " WITH PASSWORD='" PWD1 "' MUST_CHANGE, "
			   "DEFAULT_DATABASE = tempdb, CHECK_EXPIRATION = ON", "SENo");
	if (rc == SQL_ERROR) {
		odbc_read_error();
		odbc_disconnect();
		if (strstr(odbc_err, "MUST_CHANGE") == NULL)
			return 1;
		odbc_test_skipped();
		return 0;
	}
	swap_conn();

	/* login with new password should fail */
	strcpy(common_pwd.user, USER);
	strcpy(common_pwd.password, PWD1);

	CHKAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocHandle(SQL_HANDLE_DBC, odbc_env, &odbc_conn, "S");
	CHKConnect(T(common_pwd.server), SQL_NTS, T(common_pwd.user), SQL_NTS, T(common_pwd.password), SQL_NTS, "E");
	odbc_read_error();
	if (strcmp(odbc_sqlstate, "42000") != 0 ||
	    strstr(odbc_err, USER) == NULL || strstr(odbc_err, "The password of the account must be changed") == NULL) {
		fprintf(stderr, "Unexpected sql state %s returned\n", odbc_sqlstate);
		odbc_disconnect();
		return 1;
	}
	odbc_disconnect();

	/* login and change password */
	odbc_set_conn_attr = my_attrs;
	odbc_connect();
	odbc_disconnect();

	/* login wiht new password */
	odbc_set_conn_attr = NULL;
	odbc_connect();
	odbc_disconnect();

	/* drop created login */
	tds_sleep_ms(500);	/* give time for logoff */
	swap_conn();
	odbc_command("DROP LOGIN " USER);
	odbc_disconnect();

	return 0;
}
