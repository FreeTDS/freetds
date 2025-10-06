#include "common.h"
#include <freetds/macros.h>

/* test conversion of Hebrew characters (which have shift sequences) */

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

static void
hexdump(FILE* fp, const char* prefix, const void* p, size_t n)
{
	const unsigned char* cp = p;

	fprintf(fp, "%s", prefix);
	while (n--)
		fprintf(fp, "%02X", *cp++);
	fprintf(fp, "\n");
}

TEST_MAIN()
{
	char tmp[1024];
	char out[TDS_VECTOR_SIZE(column_names)][32];
	SQLLEN n_len[TDS_VECTOR_SIZE(column_names)];
	const column_t *p;
	int n;

	odbc_use_version3 = true;
	odbc_conn_additional_params = "ClientCharset=UTF-8;";

	odbc_connect();
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

	/* test conversions in libTDS
	 *
	 * Because column type is VARCHAR with collation defined, SQL Server
	 * returns the data as CP1255 (hebrew) or GB18030 (CN), and FreeTDS's
	 * built-in iconv doesn't support those. So you need iconv installed
	 * in Windows to pass this test.
	 *
	 * Using NVARCHAR makes this test work with FreeTDS's built-in iconv
	 * as it causes the server to return the data in wide characters.
	*/
#if HAVE_ICONV
	odbc_command("SELECT hebrew, cn FROM #tmp ORDER BY i");
#else
	fprintf(stderr, "Bypassing Hebrew MBCS test since iconv not installed (Forcing server to send UTF-16).\n");
	odbc_command("SELECT CAST(hebrew AS NVARCHAR(20)), CAST(cn AS NVARCHAR(20)) FROM #tmp ORDER BY i");
#endif
	/* insert with SQLPrepare/SQLBindParameter/SQLExecute */
	for (n = 0; n < TDS_VECTOR_SIZE(column_names); ++n)
		CHKBindCol(n+1, SQL_C_CHAR, out[n], sizeof(out[0]), &n_len[n], "S");
	for (n = 0, p = columns; p[n].hex; ++n) {
		memset(out, 0, sizeof(out));
		CHKFetch("S");
		if (n_len[p[n].num] != strlen(p[n].out) || strcmp(p[n].out, out[p[n].num]) != 0) {
			size_t len = strlen(p[n].out);

			fprintf(stderr, "Wrong row %d %s\n", n+1, out[p[n].num]);
			hexdump(stderr, "Expect: ", p[n].out, len);
			hexdump(stderr, "Got   : ", out[p[n].num], strlen(out[p[n].num]));
			odbc_disconnect();
			return 1;
		}
	}

	odbc_disconnect();
	printf("Done.\n");
	return 0;
}

