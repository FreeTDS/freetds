#include "common.h"

/* test conversion of Hebrew characters (which have shift sequences) */
static char software_version[] = "$Id: utf8_2.c,v 1.1 2008-10-06 13:45:15 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void init_connect(void);

static void
init_connect(void)
{
	if (SQLAllocEnv(&Environment) != SQL_SUCCESS) {
		printf("Unable to allocate env\n");
		exit(1);
	}
	SQLSetEnvAttr(Environment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	if (SQLAllocConnect(Environment, &Connection) != SQL_SUCCESS) {
		printf("Unable to allocate connection\n");
		SQLFreeEnv(Environment);
		exit(1);
	}
}

static const char * const strings[] = {
	"\xd7\x9e\xd7\x99\xd7\x93\xd7\xa2",
	"info",
	"\xd7\x98\xd7\xa7\xd7\xa1\xd7\x98",
	"\xd7\x90\xd7\x91\xd7\x9b",
	NULL
};

/* same strings in hex */
static const char * const strings_hex[] = {
	"0xde05d905d305e205",
	"0x69006e0066006f00",
	"0xd805e705e105d805",
	"0xd005d105db05",
	NULL
};

int
main(int argc, char *argv[])
{
	char tmp[128];
	char out[32];
	SQLLEN n_len;
	int res;
	SQLSMALLINT len;
	const char * const*p;
	int n;

	if (read_login_info())
		exit(1);

	/* connect string using DSN */
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;ClientCharset=UTF-8;", SERVER, USER, PASSWORD, DATABASE);
	res = SQLDriverConnect(Connection, NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to open data source (ret=%d)\n", res);
		CheckReturn();
		return 1;
	}
	if (!driver_is_freetds()) {
		Disconnect();
		printf("Driver is not FreeTDS, exiting\n");
		return 0;
	}

	memset(tmp, 0, sizeof(tmp));
	SQLGetInfo(Connection, SQL_DBMS_VER, tmp, sizeof(tmp), &len);
	if (!db_is_microsoft() || (strncmp(tmp, "08.00.", 6) != 0 && strncmp(tmp, "09.00.", 6) != 0)) {
		Disconnect();
		printf("Test for MSSQL only\n");
		return 0;
	}

	if (SQLAllocStmt(Connection, &Statement) != SQL_SUCCESS) {
		printf("Unable to allocate statement\n");
		CheckReturn();
	}

	/* create test table */
	Command(Statement, "CREATE TABLE #tmpHebrew (i INT, v VARCHAR(10) COLLATE Hebrew_CI_AI)");

	/* insert with INSERT statements */
	for (n = 0, p = strings_hex; p[n]; ++n) {
		sprintf(tmp, "INSERT INTO #tmpHebrew VALUES(%d, CAST(%s AS NVARCHAR(10)))", n+1, p[n]);
		Command(Statement, tmp);
	}

	/* test conversions in libTDS */
	Command(Statement, "SELECT v FROM #tmpHebrew");

	/* insert with SQLPrepare/SQLBindParameter/SQLExecute */
	CHK(SQLBindCol, (Statement, 1, SQL_C_CHAR, out, sizeof(out), &n_len));
	for (n = 0, p = strings; p[n]; ++n) {
		CHK(SQLFetch, (Statement));
		if (n_len != strlen(p[n]) || strcmp(p[n], out) != 0) {
			fprintf(stderr, "Wrong row %s\n", out);
			Disconnect();
			return 1;
		}
	}

	Disconnect();
	printf("Done.\n");
	return 0;
}

