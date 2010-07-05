#include "common.h"

/*
 * Test setting current "catalog" before and after connection using
 * either SQLConnect and SQLDriverConnect
 */

static char software_version[] = "$Id: connect2.c,v 1.8 2010-07-05 09:20:32 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int failed = 0;

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	CHKAllocConnect(&odbc_conn, "S");
}

static void
normal_connect(void)
{
	CHKR(SQLConnect, (odbc_conn, (SQLCHAR *) odbc_server, SQL_NTS, (SQLCHAR *) odbc_user, SQL_NTS, (SQLCHAR *) odbc_password, SQL_NTS), "SI");
}

static void
driver_connect(const char *conn_str)
{
	char tmp[1024];
	SQLSMALLINT len;

	CHKDriverConnect(NULL, (SQLCHAR *) conn_str, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT, "SI");
}

static void
check_dbname(const char *dbname)
{
	SQLINTEGER len;
	char out[512];

	len = sizeof(out);
	CHKGetConnectAttr(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER) out, sizeof(out), &len, "SI");

	if (strcmp(out, dbname) != 0) {
		fprintf(stderr, "Current database (%s) is not %s\n", out, dbname);
		failed = 1;
	}
}

static void
set_dbname(const char *dbname)
{
	CHKSetConnectAttr(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER) dbname, strlen(dbname), "SI");
}

int
main(int argc, char *argv[])
{
	char tmp[1024];

	if (odbc_read_login_info())
		exit(1);

	/* try setting db name before connect */
	printf("SQLConnect before 1..\n");
	init_connect();
	set_dbname("master");
	normal_connect();
	check_dbname("master");

	/* check change after connection */
	printf("SQLConnect after..\n");
	set_dbname("tempdb");
	check_dbname("tempdb");

	printf("SQLConnect after not existing..\n");
	strcpy(tmp, "IDontExist");
	CHKSetConnectAttr(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER) tmp, strlen(tmp), "E");
	check_dbname("tempdb");

	odbc_disconnect();

	/* try setting db name before connect */
	printf("SQLConnect before 2..\n");
	init_connect();
	set_dbname("tempdb");
	normal_connect();
	check_dbname("tempdb");
	odbc_disconnect();

	/* try connect string with using DSN */
	printf("SQLDriverConnect before 1..\n");
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", odbc_server, odbc_user, odbc_password, odbc_database);
	init_connect();
	set_dbname("master");
	driver_connect(tmp);
	check_dbname(odbc_database);
	odbc_disconnect();

	/* try connect string with using DSN */
	printf("SQLDriverConnect before 2..\n");
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;", odbc_server, odbc_user, odbc_password);
	init_connect();
	set_dbname("tempdb");
	driver_connect(tmp);
	check_dbname("tempdb");
	odbc_disconnect();

	if (failed) {
		printf("Some tests failed\n");
		return 1;
	}

	printf("Done.\n");
	return 0;
}
