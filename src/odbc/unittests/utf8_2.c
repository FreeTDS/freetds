#include "common.h"
#include <freetds/macros.h>

/* test conversion of Hebrew characters (which have shift sequences) */

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocConnect(&odbc_conn, "S");
}

static const char * const column_names[] = {
	"hebrew",
	"cn"
};

typedef struct {
	/* number of column */
	int num;
	/* hex representation, used during insert */
	const char *hex;
	/* output */
	const char *out;
} column_t;

static const column_t columns[] = {
	{ 0, "0xde05d905d305e205", "\xd7\x9e\xd7\x99\xd7\x93\xd7\xa2" },
	{ 0, "0x69006e0066006f00", "info", },
	{ 0, "0xd805e705e105d805", "\xd7\x98\xd7\xa7\xd7\xa1\xd7\x98", },
	{ 0, "0xd005d105db05", "\xd7\x90\xd7\x91\xd7\x9b", },
	{ 1, "0xf78b7353d153278d3a00228c228c56e02000",
	     "\xe8\xaf\xb7\xe5\x8d\xb3\xe5\x8f\x91\xe8\xb4\xa7\x3a\xe8\xb0\xa2\xe8\xb0\xa2\xee\x81\x96\x20", },
	{ 0, NULL, NULL },
};

int
main(int argc, char *argv[])
{
	char tmp[512*4+64];
	char out[TDS_VECTOR_SIZE(column_names)][32];
	SQLLEN n_len[TDS_VECTOR_SIZE(column_names)];
	SQLSMALLINT len;
	const column_t *p;
	int n;

	if (odbc_read_login_info())
		exit(1);

	/* connect string using DSN */
	init_connect();
	sprintf(tmp, "DSN=%s;UID=%s;PWD=%s;DATABASE=%s;ClientCharset=UTF-8;", odbc_server, odbc_user, odbc_password, odbc_database);
	CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp)/sizeof(SQLTCHAR), &len, SQL_DRIVER_NOPROMPT, "SI");
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		printf("Driver is not FreeTDS, exiting\n");
		odbc_test_skipped();
		return 0;
	}

	if (!odbc_db_is_microsoft() || odbc_db_version_int() < 0x08000000u || odbc_tds_version() < 0x701) {
		odbc_disconnect();
		/* protocol till 7.1 does not support telling encoding so we
		 * cannot understand how the string is encoded
		 */
		printf("Test for MSSQL only using protocol 7.1\n");
		odbc_test_skipped();
		return 0;
	}

	CHKAllocStmt(&odbc_stmt, "S");

	/* create test table */
	odbc_command("CREATE TABLE #tmp (i INT"
		     ", hebrew VARCHAR(20) COLLATE Hebrew_CI_AI NULL"
		     ", cn VARCHAR(20) COLLATE Chinese_PRC_CI_AS NULL"
		     ")");

	/* insert with INSERT statements */
	for (n = 0, p = columns; p[n].hex; ++n) {
		sprintf(tmp, "INSERT INTO #tmp(i, %s) VALUES(%d, CAST(%s AS NVARCHAR(20)))",
			     column_names[p[n].num], n+1, p[n].hex);
		odbc_command(tmp);
	}

	/* test conversions in libTDS */
	odbc_command("SELECT hebrew, cn FROM #tmp ORDER BY i");

	/* insert with SQLPrepare/SQLBindParameter/SQLExecute */
	for (n = 0; n < TDS_VECTOR_SIZE(column_names); ++n)
		CHKBindCol(n+1, SQL_C_CHAR, out[n], sizeof(out[0]), &n_len[n], "S");
	for (n = 0, p = columns; p[n].hex; ++n) {
		memset(out, 0, sizeof(out));
		CHKFetch("S");
		if (n_len[p[n].num] != strlen(p[n].out) || strcmp(p[n].out, out[p[n].num]) != 0) {
			fprintf(stderr, "Wrong row %d %s\n", n+1, out[p[n].num]);
			odbc_disconnect();
			return 1;
		}
	}

	odbc_disconnect();
	printf("Done.\n");
	return 0;
}

