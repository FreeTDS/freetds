#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "common.h"

static char software_version[] = "$Id: common.c,v 1.15 2003-02-27 15:23:54 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

HENV Environment;
HDBC Connection;
HSTMT Statement;

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];

int
read_login_info(void)
{
	FILE *in;
	char line[512];
	char *s1, *s2;

	in = fopen("../../../PWD", "r");
	if (!in) {
		fprintf(stderr, "Can not open PWD file\n\n");
		return 1;
	}
	while (fgets(line, 512, in)) {
		s1 = strtok(line, "=");
		s2 = strtok(NULL, "\n");
		if (!s1 || !s2)
			continue;
		if (!strcmp(s1, "UID")) {
			strcpy(USER, s2);
		} else if (!strcmp(s1, "SRV")) {
			strcpy(SERVER, s2);
		} else if (!strcmp(s1, "PWD")) {
			strcpy(PASSWORD, s2);
		} else if (!strcmp(s1, "DB")) {
			strcpy(DATABASE, s2);
		}
	}
	return 0;
}

void
CheckReturn(void)
{
	SQLSMALLINT handletype;
	SQLHANDLE handle;
	SQLRETURN ret;
	unsigned char sqlstate[6];
	unsigned char msg[256];


	if (Statement) {
		handletype = SQL_HANDLE_STMT;
		handle = Statement;
	} else if (Connection) {
		handletype = SQL_HANDLE_DBC;
		handle = Connection;
	} else {
		handletype = SQL_HANDLE_ENV;
		handle = Environment;
	}
	ret = SQLGetDiagRec(handletype, handle, 1, sqlstate, NULL, msg, sizeof(msg), NULL);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		printf("SQL error %s -- %s\n", sqlstate, msg);
	exit(1);
}


int
Connect(void)
{

	int res;


	char command[512];

	if (read_login_info())
		exit(1);

	if (SQLAllocEnv(&Environment) != SQL_SUCCESS) {
		printf("Unable to allocate env\n");
		exit(1);
	}
	if (SQLAllocConnect(Environment, &Connection) != SQL_SUCCESS) {
		printf("Unable to allocate connection\n");
		SQLFreeEnv(Environment);
		exit(1);
	}
	printf("odbctest\n--------\n\n");
	printf("connection parameters:\nserver:   '%s'\nuser:     '%s'\npassword: '%s'\ndatabase: '%s'\n",
	       SERVER, USER, "????" /* PASSWORD */ , DATABASE);

	res = SQLConnect(Connection, (SQLCHAR*) SERVER, SQL_NTS, (SQLCHAR*) USER, SQL_NTS, (SQLCHAR*) PASSWORD, SQL_NTS);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		exit(1);
	}

	if (SQLAllocStmt(Connection, &Statement) != SQL_SUCCESS) {
		printf("Unable to allocate statement\n");
		CheckReturn();
		exit(1);
	}

	sprintf(command, "use %s", DATABASE);
	printf("%s\n", command);

	if (!SQL_SUCCEEDED(SQLExecDirect(Statement, (SQLCHAR*) command, SQL_NTS))) {
		printf("Unable to execute statement\n");
		CheckReturn();
		exit(1);
	}
	return 0;
}

int
Disconnect(void)
{
	if (Statement) {
		SQLFreeStmt(Statement, SQL_DROP);
		Statement = SQL_NULL_HSTMT;
	}

	if (Connection) {
		SQLDisconnect(Connection);
		SQLFreeConnect(Connection);
		Connection = SQL_NULL_HDBC;
	}

	if (Environment) {
		SQLFreeEnv(Environment);
		Environment = SQL_NULL_HENV;
	}
	return 0;
}

void
Command(HSTMT stmt, const char *command)
{
	printf("%s\n", command);
	if (SQLExecDirect(stmt, (SQLCHAR *) command, SQL_NTS)
	    != SQL_SUCCESS) {
		printf("Unable to execute statement\n");
		CheckReturn();
		exit(1);
	}
}
