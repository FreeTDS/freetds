#include "common.h"
#include <assert.h>

/* test conversion using SQLGetData */

TEST_MAIN()
{
	SQLLEN len;
	unsigned char buf[30];
	static const char expected[] = "\xf0\x9f\x8e\x84";
	int i;

	odbc_use_version3 = true;
	odbc_conn_additional_params = "ClientCharset=UTF-8;";

	odbc_connect();
	if (!odbc_driver_is_freetds()) {
		odbc_disconnect();
		printf("Driver is not FreeTDS, exiting\n");
		odbc_test_skipped();
		return 0;
	}

	if (!odbc_db_is_microsoft() || odbc_tds_version() < 0x700) {
		odbc_disconnect();
		/* we need NVARCHAR */
		printf("Test for MSSQL only using protocol 7.0\n");
		odbc_test_skipped();
		return 0;
	}

	CHKAllocStmt(&odbc_stmt, "S");

	/* a Christmas tree */
	odbc_command("SELECT CONVERT(NVARCHAR(10), CONVERT(VARBINARY(20), 0x3CD884DF))");

	CHKFetch("S");

	/* read one byte at a time and test it */
	for (i = 0; i < 4; ++i) {
		memset(buf, '-', sizeof(buf));
		CHKGetData(1, SQL_C_CHAR, buf, 2, &len, i < 3 ? "I" : "S");
		printf("res %ld buf { 0x%02x, 0x%02x }\n", (long int) len, buf[0], buf[1]);
		assert(len == SQL_NO_TOTAL || len == 4 - i);
		assert(buf[0] == (unsigned char) expected[i]);
		assert(buf[1] == 0);
	}
	CHKGetData(1, SQL_C_CHAR, buf, 2, &len, "No");

	odbc_reset_statement();

#define CN_STRING \
	"202020202052656974657261746520486f6c642028324829207261a174696e6720a15820" \
	"a14ea16fa172a173a174a161a172a1207265706f72746564206120736574206f6620756e" \
	"6578636974696e6720726573756c74732077697468206d6f646573742067726f77746820" \
	"696e20726576656e756520616e6420626f74746f6d206c696e652e20457870616e73696f" \
	"6e20696e746f2074686520646f6d6573"

	/* insert does not change as much as CONVERT so insert first into a new table */
	odbc_command("CREATE TABLE #tmp1(c VARCHAR(200) COLLATE Chinese_PRC_CI_AS NULL)");
	odbc_command("INSERT INTO #tmp1 VALUES(CONVERT(VARBINARY(200), 0x" CN_STRING "))");
	odbc_command("SELECT c FROM #tmp1");
	CHKFetch("S");
	for (i = 0; i < 5; ++i) {
		memset(buf, 0, sizeof(buf));
		CHKGetData(1, SQL_C_CHAR, buf, sizeof(buf), &len, "I");
		printf("loop %d output '%s'\n", i, buf);
		assert(strlen((char *) buf) == sizeof(buf) - 1);
	}
	CHKGetData(1, SQL_C_CHAR, buf, sizeof(buf), &len, "S");

	odbc_disconnect();
	return 0;
}

