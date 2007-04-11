#include "common.h"

/*
 * Test setting current "catalog" before and after connection using
 * either SQLConnect and SQLDriverConnect
 */

static char software_version[] = "$Id: connect2.c,v 1.1 2007-04-11 07:10:20 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void init_connect(void);

static void
init_connect(void)
{
	if (SQLAllocEnv(&Environment) != SQL_SUCCESS) {
		printf("Unable to allocate env\n");
		exit(1);
	}
	if (SQLAllocConnect(Environment, &Connection) != SQL_SUCCESS) {
		printf("Unable to allocate connection\n");
		SQLFreeEnv(Environment);
		exit(1);
	}
}

static void
normal_connect(void)
{
	int res;

	res = SQLConnect(Connection, (SQLCHAR *) SERVER, SQL_NTS, (SQLCHAR *) USER, SQL_NTS, (SQLCHAR *) PASSWORD, SQL_NTS);
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}
}

static void
driver_connect(const char *conn_str)
{
	char tmp[1024];
	SQLSMALLINT len;
	int res;

	res = SQLDriverConnect(Connection, NULL, (SQLCHAR *) conn_str, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}
}

static void
check_dbname(const char *dbname)
{
	SQLINTEGER len;
	char out[512];
	int res;

	len = sizeof(out);
	res = SQLGetConnectAttr(Connection, SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER) out, sizeof(out), &len);
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to get database name to %s\n", dbname);
		CheckReturn();
		exit(1);
	}

	if (strcmp(out, dbname) != 0) {
		fprintf(stderr, "Current database (%s) is not %s\n", out, dbname);
		exit(1);
	}
}

static void
set_dbname(const char *dbname)
{
	int res;

	res = SQLSetConnectAttr(Connection, SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER) dbname, strlen(dbname));
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to set database name to %s\n", dbname);
		CheckReturn();
		exit(1);
	}
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
	init_connect();
	set_dbname("tempdb");
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
	check_dbname("master");
	Disconnect();

	/* try connect string with using DSN */
	printf("SQLDriverConnect before 2..\n");
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	init_connect();
	set_dbname("tempdb");
	driver_connect(tmp);
	check_dbname("tempdb");
	Disconnect();

	printf("Done.\n");
	return 0;
}
