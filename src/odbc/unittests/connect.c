#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "common.h"


static char software_version[] = "$Id: connect.c,v 1.2 2002-11-22 15:40:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

void
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
	SQLUSMALLINT len;

	printf("SQLConnect connect..\n");
	Connect();
	Disconnect();

	/* try connect string with using DSN */
	printf("connect string DSN connect..\n");
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;", SERVER, USER, PASSWORD, DATABASE);
	res = SQLDriverConnect(Connection, NULL, tmp, SQL_NTS, tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (res != SQL_SUCCESS) {
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
	res = SQLDriverConnect(Connection, NULL, tmp, SQL_NTS, tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (res != SQL_SUCCESS) {
		printf("Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}
	Disconnect();

	printf("Done.\n");
	return 0;
}
