#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <ctype.h>

#ifndef _WIN32
#include "tds_sysdep_private.h"
#else
#define TDS_SDIR_SEPARATOR "\\"
#endif

static char software_version[] = "$Id: common.c,v 1.56 2010-01-10 14:43:11 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

HENV Environment;
HDBC Connection;
HSTMT Statement;
int use_odbc_version3 = 0;
void (*odbc_set_conn_attr)(void) = NULL;

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];
char DRIVER[1024];

#ifndef _WIN32
static int
check_lib(char *path, const char *file)
{
	int len = strlen(path);
	FILE *f;

	strcat(path, file);
	f = fopen(path, "rb");
	if (f) {
		fclose(f);
		return 1;
	}
	path[len] = 0;
	return 0;
}
#endif

/* some platforms do not have setenv, define a replacement */
#if !HAVE_SETENV
void
odbc_setenv(const char *name, const char *value, int overwrite)
{
#if HAVE_PUTENV
	char buf[1024];

	sprintf(buf, "%s=%s", name, value);
	putenv(buf);
#endif
}
#endif

int
read_login_info(void)
{
	static const char *PWD = "../../../PWD";
	FILE *in = NULL;
	char line[512];
	char *s1, *s2;
#ifndef _WIN32
	char path[1024];
	int len;
#endif

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	s1 = getenv("TDSPWDFILE");
	if (s1 && s1[0])
		in = fopen(s1, "r");
	if (!in)
		in = fopen(PWD, "r");
	if (!in)
		in = fopen("PWD", "r");

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
	fclose(in);

#ifndef _WIN32
	/* find our driver */
	if (!getcwd(path, sizeof(path)))
		return 0;
#ifdef __VMS
	{
	    /* A hard-coded driver path has to be in unix syntax to be recognized as such. */
	    const char *unixspec = decc$translate_vms(path);
	    if ( (int)unixspec != 0 && (int)unixspec != -1 ) strcpy(path, unixspec);
	}
#endif
	len = strlen(path);
	if (len < 10 || strcmp(path + len - 10, "/unittests") != 0)
		return 0;
	path[len - 9] = 0;
	/* TODO this must be extended with all possible systems... */
	if (!check_lib(path, ".libs/libtdsodbc.so") && !check_lib(path, ".libs/libtdsodbc.sl")
	    && !check_lib(path, ".libs/libtdsodbc.dll") && !check_lib(path, ".libs/libtdsodbc.dylib")
	    && !check_lib(path, "_libs/libtdsodbc.exe"))
		return 0;
	strcpy(DRIVER, path);

	/* craft out odbc.ini, avoid to read wrong one */
	in = fopen("odbc.ini", "w");
	if (in) {
		fprintf(in, "[%s]\nDriver = %s\nDatabase = %s\nServername = %s\n", SERVER, DRIVER, DATABASE, SERVER);
		fclose(in);
		setenv("ODBCINI", "./odbc.ini", 1);
		setenv("SYSODBCINI", "./odbc.ini", 1);
	}
#endif
	return 0;
}

void
ReportError(const char *errmsg, int line, const char *file)
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
	if (errmsg[0]) {
		if (line)
			fprintf(stderr, "%s:%d %s\n", file, line, errmsg);
		else
			fprintf(stderr, "%s\n", errmsg);
	}
	ret = SQLGetDiagRec(handletype, handle, 1, sqlstate, NULL, msg, sizeof(msg), NULL);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		fprintf(stderr, "SQL error %s -- %s\n", sqlstate, msg);
	Disconnect();
	exit(1);
}

static void
ReportODBCError(const char *errmsg, SQLSMALLINT handletype, SQLHANDLE handle, SQLRETURN rc, int line, const char *file)
{
	SQLRETURN ret;
	unsigned char sqlstate[6];
	unsigned char msg[256];


	if (errmsg[0]) {
		if (line)
			fprintf(stderr, "%s:%d rc=%d %s\n", file, line, (int) rc, errmsg);
		else
			fprintf(stderr, "rc=%d %s\n", (int) rc, errmsg);
	}
	ret = SQLGetDiagRec(handletype, handle, 1, sqlstate, NULL, msg, sizeof(msg), NULL);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		fprintf(stderr, "SQL error %s -- %s\n", sqlstate, msg);
	Disconnect();
	exit(1);
}

int
Connect(void)
{
	char command[512];

	if (read_login_info())
		exit(1);

	if (use_odbc_version3) {
		CHKAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &Environment, "S");
		SQLSetEnvAttr(Environment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
		CHKAllocHandle(SQL_HANDLE_DBC, Environment, &Connection, "S");
	} else {
		CHKAllocEnv(&Environment, "S");
		CHKAllocConnect(&Connection, "S");
	}

	printf("odbctest\n--------\n\n");
	printf("connection parameters:\nserver:   '%s'\nuser:     '%s'\npassword: '%s'\ndatabase: '%s'\n",
	       SERVER, USER, "????" /* PASSWORD */ , DATABASE);

	if (odbc_set_conn_attr)
		(*odbc_set_conn_attr)();

	CHKR(SQLConnect, (Connection, (SQLCHAR *) SERVER, SQL_NTS, (SQLCHAR *) USER, SQL_NTS, (SQLCHAR *) PASSWORD, SQL_NTS), "SI");

	CHKAllocStmt(&Statement, "S");

	sprintf(command, "use %s", DATABASE);
	printf("%s\n", command);

	CHKExecDirect((SQLCHAR *) command, SQL_NTS, "SI");

#ifndef TDS_NO_DM
	/* unixODBC seems to require it */
	SQLMoreResults(Statement);
#endif
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

SQLRETURN
CommandWithResult(HSTMT stmt, const char *command)
{
	printf("%s\n", command);
	return SQLExecDirect(stmt, (SQLCHAR *) command, SQL_NTS);
}

static int ms_db = -1;
int
db_is_microsoft(void)
{
	char buf[64];
	SQLSMALLINT len;
	int i;

	if (ms_db < 0) {
		buf[0] = 0;
		SQLGetInfo(Connection, SQL_DBMS_NAME, buf, sizeof(buf), &len);
		for (i = 0; buf[i]; ++i)
			buf[i] = tolower(buf[i]);
		ms_db = (strstr(buf, "microsoft") != NULL);
	}
	return ms_db;
}

static int freetds_driver = -1;
int
driver_is_freetds(void)
{
	char buf[64];
	SQLSMALLINT len;
	int i;

	if (freetds_driver < 0) {
		buf[0] = 0;
		SQLGetInfo(Connection, SQL_DRIVER_NAME, buf, sizeof(buf), &len);
		for (i = 0; buf[i]; ++i)
			buf[i] = tolower(buf[i]);
		freetds_driver = (strstr(buf, "tds") != NULL);
	}
	return freetds_driver;
}

static char db_str_version[32];

const char *db_version(void)
{
	SQLSMALLINT version_len;

	if (!db_str_version[0])
		CHKR(SQLGetInfo, (Connection, SQL_DBMS_VER, db_str_version, sizeof(db_str_version), &version_len), "S");

	return db_str_version;
}

unsigned int db_version_int(void)
{
	unsigned int h, l;
	if (sscanf(db_version(), "%u.%u.", &h, &l) != 2) {
		fprintf(stderr, "Wrong db version: %s\n", db_version());
		Disconnect();
		exit(1);
	}

	return (h << 24) | ((l & 0xFFu) << 16);
}

void
CheckCols(int n, int line, const char * file)
{
	SQLSMALLINT cols;

	if (n < 0) {
		CHKNumResultCols(&cols, "E");
		return;
	}
	CHKNumResultCols(&cols, "S");
	if (cols != n) {
		fprintf(stderr, "%s:%d: Expected %d columns returned %d\n", file, line, n, (int) cols);
		Disconnect();
		exit(1);
	}
}

void
CheckRows(int n, int line, const char * file)
{
	SQLLEN rows;

	if (n < -1) {
		CHKRowCount(&rows, "E");
		return;
	}

	CHKRowCount(&rows, "S");
	if (rows != n) {
		fprintf(stderr, "%s:%d: Expected %d rows returned %d\n", file, line, n, (int) rows);
		Disconnect();
		exit(1);
	}
}

void
ResetStatementProc(SQLHSTMT *stmt, const char *file, int line)
{
	SQLFreeStmt(*stmt, SQL_DROP);
	*stmt = SQL_NULL_HSTMT;
	CheckRes(file, line, SQLAllocStmt(Connection, stmt), SQL_HANDLE_DBC, Connection, "SQLAllocStmt", "S");
}

void
CheckCursor(void)
{
	SQLRETURN retcode;

	retcode = SQLSetStmtAttr(Statement, SQL_ATTR_CONCURRENCY, (SQLPOINTER) SQL_CONCUR_ROWVER, 0);
	if (retcode != SQL_SUCCESS) {
		char output[256];
		unsigned char sqlstate[6];

		CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *) output, sizeof(output), NULL, "S");
		sqlstate[5] = 0;
		if (strcmp((const char*) sqlstate, "01S02") == 0) {
			printf("Your connection seems to not support cursors, probably you are using wrong protocol version or Sybase\n");
			Disconnect();
			exit(0);
		}
		ReportODBCError("SQLSetStmtAttr", SQL_HANDLE_STMT, Statement, retcode, __LINE__, __FILE__);
	}
	ResetStatement();
}

SQLRETURN
CheckRes(const char *file, int line, SQLRETURN rc, SQLSMALLINT handle_type, SQLHANDLE handle, const char *func, const char *res)
{
	const char *p = res;
	for (;;) {
		if (*p == 'S') {
			if (rc == SQL_SUCCESS)
				return rc;
			++p;
		} else if (*p == 'I') {
			if (rc == SQL_SUCCESS_WITH_INFO)
				return rc;
			++p;
		} else if (*p == 'E') {
			if (rc == SQL_ERROR)
				return rc;
			++p;
		} else if (strncmp(p, "No", 2) == 0) {
			if (rc == SQL_NO_DATA)
				return rc;
			p += 2;
		} else if (strncmp(p, "Ne", 2) == 0) {
			if (rc == SQL_NEED_DATA)
				return rc;
			p += 2;
		} else if (!*p) {
			break;
		} else {
			ReportError("Wrong results specified", line, file);
			return rc;
		}
	}
	ReportODBCError(func, handle_type, handle, rc, line, file);
	return rc;
}

SQLSMALLINT
AllocHandleErrType(SQLSMALLINT type)
{
	switch (type) {
	case SQL_HANDLE_DESC:
		return SQL_HANDLE_STMT;
	case SQL_HANDLE_STMT:
		return SQL_HANDLE_DBC;
	case SQL_HANDLE_DBC:
		return SQL_HANDLE_ENV;
	}
	return 0;
}

SQLRETURN
CommandProc(HSTMT stmt, const char *command, const char *file, int line, const char *res)
{
	printf("%s\n", command);
	return CheckRes(file, line, SQLExecDirect(stmt, (SQLCHAR *) command, SQL_NTS), SQL_HANDLE_STMT, stmt, "Command", res);
}

