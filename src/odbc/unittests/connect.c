#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "common.h"


static char software_version[] = "$Id: connect.c,v 1.5 2003-02-27 15:23:55 freddy77 Exp $";
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

int
main(int argc, char *argv[])
{
	int res;
	char tmp[2048];
	SQLSMALLINT len;

	printf("SQLConnect connect..\n");
	Connect();
	Disconnect();

	/* try connect string with using DSN */
	printf("connect string DSN connect..\n");
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	res = SQLDriverConnect(Connection, NULL, (SQLCHAR*) tmp, SQL_NTS, (SQLCHAR*) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}
	Disconnect();

	/* try connect string using old SERVERNAME specification */
	printf("connect string SERVERNAME connect..\n");
	printf("odbcinst.ini must be configured with FreeTDS driver..\n");
	init_connect();
	sprintf(tmp, "DRIVER=FreeTDS;SERVERNAME=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	res = SQLDriverConnect(Connection, NULL, (SQLCHAR*) tmp, SQL_NTS, (SQLCHAR*) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}
	Disconnect();

	printf("Done.\n");
	return 0;
}
