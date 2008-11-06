#include "common.h"

/*
 * Test setting current "catalog" before and after connection using
 * either SQLConnect and SQLDriverConnect
 */

static char software_version[] = "$Id: connect2.c,v 1.7 2008-11-06 15:56:39 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int failed = 0;

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&Environment, "S");
	CHKAllocConnect(&Connection, "S");
}

static void
normal_connect(void)
{
	CHKR(SQLConnect, (Connection, (SQLCHAR *) SERVER, SQL_NTS, (SQLCHAR *) USER, SQL_NTS, (SQLCHAR *) PASSWORD, SQL_NTS), "SI");
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

	if (read_login_info())
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

	Disconnect();

	/* try setting db name before connect */
	printf("SQLConnect before 2..\n");
	init_connect();
	set_dbname("tempdb");
	normal_connect();
	check_dbname("tempdb");
	Disconnect();

	/* try connect string with using DSN */
	printf("SQLDriverConnect before 1..\n");
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	init_connect();
	set_dbname("master");
	driver_connect(tmp);
	check_dbname(DATABASE);
	Disconnect();

	/* try connect string with using DSN */
	printf("SQLDriverConnect before 2..\n");
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;", SERVER, USER, PASSWORD);
	init_connect();
	set_dbname("tempdb");
	driver_connect(tmp);
	check_dbname("tempdb");
	Disconnect();

	if (failed) {
		printf("Some tests failed\n");
		return 1;
	}

	printf("Done.\n");
	return 0;
}
