#include "common.h"

/* test binding with UTF-8 encoding */
static char software_version[] = "$Id: utf8.c,v 1.2 2008-08-17 07:44:45 freddy77 Exp $";
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

static void
CheckNoRow(const char *query)
{
	SQLRETURN retcode;

	retcode = SQLExecDirect(Statement, (SQLCHAR *) query, SQL_NTS);
	if (retcode == SQL_NO_DATA)
		return;

	if (!SQL_SUCCEEDED(retcode))
		ODBC_REPORT_ERROR("SQLExecDirect");

	do {
		SQLSMALLINT cols;

		retcode = SQLNumResultCols(Statement, &cols);
		if (retcode != SQL_SUCCESS || cols != 0) {
			fprintf(stderr, "Data not expected here, query:\n\t%s\n", query);
			exit(1);
		}
	} while ((retcode = SQLMoreResults(Statement)) == SQL_SUCCESS);
	if (retcode != SQL_NO_DATA)
		ODBC_REPORT_ERROR("SQLMoreResults");
}

/* test table name, it contains two japanese characters */
static const char table_name[] = "mytab\xe7\x8e\x8b\xe9\xb4\xbb";

static const char * const strings[] = {
	/* ascii */
	"aaa", "aaa",
	/* latin 1*/
	"abc\xc3\xa9\xc3\xa1\xc3\xb4", "abc\xc3\xa9\xc3\xae\xc3\xb4",
	/* Japanese... */
	"abc\xe7\x8e\x8b\xe9\xb4\xbb", "abc\xe7\x8e\x8b\xe9\xb4\xbb\xe5\x82\x91\xe7\x8e\x8b\xe9\xb4\xbb\xe5\x82\x91",
	NULL, NULL
};

/* same strings in hex */
static const char * const strings_hex[] = {
	/* ascii */
	"0x610061006100", "0x610061006100",
	/* latin 1*/
	"0x610062006300e900e100f400", "0x610062006300e900ee00f400",
	/* Japanese... */
	"0x6100620063008b733b9d", "0x6100620063008b733b9d91508b733b9d9150",
	NULL, NULL
};

int
main(int argc, char *argv[])
{
	int res;
	char tmp[2048];
	SQLSMALLINT len;
	const char * const*p;
	SQLINTEGER n;
	SQLLEN n_len;

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
	sprintf(tmp, "IF OBJECT_ID(N'%s') IS NOT NULL DROP TABLE %s", table_name, table_name);
	Command(Statement, tmp);
	sprintf(tmp, "CREATE TABLE %s (k int, c NCHAR(10), vc NVARCHAR(10))", table_name);
	Command(Statement, tmp);

	/* insert with INSERT statements */
	for (n = 1, p = strings; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "INSERT INTO %s VALUES (%d,N'%s',N'%s')", table_name, n, p[0], p[1]);
		Command(Statement, tmp);
	}

	/* check rows */
	for (n = 1, p = strings_hex; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "IF NOT EXISTS(SELECT * FROM %s WHERE k = %d AND c = %s AND vc = %s) SELECT 1", table_name, n, p[0], p[1]);
		CheckNoRow(tmp);
	}
	sprintf(tmp, "DELETE FROM %s", table_name);
	Command(Statement, tmp);

	/* insert with SQLPrepare/SQLBindParameter/SQLExecute */
	sprintf(tmp, "INSERT INTO %s VALUES(?,?,?)", table_name);
	CHK(SQLPrepare, (Statement, (SQLCHAR *) tmp, SQL_NTS));
	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_LONG,
		SQL_INTEGER, 0, 0, &n, 0, &n_len));
	n_len = sizeof(n);
	
	for (n = 1, p = strings; p[0] && p[1]; p += 2, ++n) {
		SQLLEN s1_len, s2_len;

		CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_WCHAR, 40, 0, (void *) p[0], 0, &s1_len));
		/* FIXME this with SQL_VARCHAR produce wrong protocol data */
		CHK(SQLBindParameter, (Statement, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_WVARCHAR, 40, 0, (void *) p[1], 0, &s2_len));
		s1_len = strlen(p[0]);
		s2_len = strlen(p[1]);
		printf("insert #%d\n", n);
		CHK(SQLExecute, (Statement));
	}

	/* check rows */
	for (n = 1, p = strings_hex; p[0] && p[1]; p += 2, ++n) {
		sprintf(tmp, "IF NOT EXISTS(SELECT * FROM %s WHERE k = %d AND c = %s AND vc = %s) SELECT 1", table_name, n, p[0], p[1]);
		CheckNoRow(tmp);
	}

	/* cleanup */
	sprintf(tmp, "IF OBJECT_ID(N'%s') IS NOT NULL DROP TABLE %s", table_name, table_name);
	Command(Statement, tmp);

	Disconnect();
	printf("Done.\n");
	return 0;
}

