#include "common.h"
#include <assert.h>

/* Test various bind type */

static char software_version[] = "$Id: data.c,v 1.9 2004-12-08 20:30:06 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int result = 0;
static char sbuf[1024];

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, const char *expected)
{
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* execute a select to get data as wire */
	sprintf(sbuf, "SELECT CONVERT(%s, '%s') AS data", type, value_to_convert);
	Command(Statement, sbuf);
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	if (SQLFetch(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "Expected row\n");
		exit(1);
	}
	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Row not expected\n");
		exit(1);
	}
	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Recordset not expected\n");
		exit(1);
	}

	/* test results */
	sbuf[0] = 0;
	switch (out_c_type) {
	case SQL_C_NUMERIC:
		num = (SQL_NUMERIC_STRUCT *) out_buf;
		sprintf(sbuf, "%d %d %d ", num->precision, num->scale, num->sign);
		i = SQL_MAX_NUMERIC_LEN;
		for (; i > 0 && !num->val[--i];);
		for (; i >= 0; --i)
			sprintf(strchr(sbuf, 0), "%02X", num->val[i]);
		break;
	case SQL_C_BINARY:
		assert(out_len >= 0);
		for (i = 0; i < out_len; ++i)
			sprintf(strchr(sbuf, 0), "%02X", (int) out_buf[i]);
		break;
	default:
		/* not supported */
		assert(0);
		break;
	}

	if (strcmp(sbuf, expected) != 0) {
		fprintf(stderr, "Wrong result\n  Got: %s\n  Expected: %s\n", sbuf, expected);
		result = 1;
	}
}

int
main(int argc, char *argv[])
{
	int big_endian = 1;
	char version[32];
	SQLSMALLINT version_len;

	Connect();

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;
	memset(version, 0, sizeof(version));
	SQLGetInfo(Connection, SQL_DBMS_VER, version, sizeof(version), &version_len);

	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, "38 0 1 7B");

	/* all binary results */
	Test("CHAR(7)", "pippo", SQL_C_BINARY, "706970706F2020");
	Test("TEXT", "mickey", SQL_C_BINARY, "6D69636B6579");
	Test("VARCHAR(20)", "foo", SQL_C_BINARY, "666F6F");

	Test("BINARY(5)", "qwer", SQL_C_BINARY, "7177657200");
	Test("IMAGE", "cricetone", SQL_C_BINARY, "6372696365746F6E65");
	Test("VARBINARY(20)", "teo", SQL_C_BINARY, "74656F");
	/* TODO only MS ?? */
	if (db_is_microsoft())
		Test("TIMESTAMP", "abcdefghi", SQL_C_BINARY, "6162636465666768");

	Test("DATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, big_endian ? "0000949700FBAA2C" : "979400002CAAFB00");
	Test("SMALLDATETIME", "2004-02-24 15:16:17", SQL_C_BINARY, big_endian ? "94970394" : "97949403");

	Test("BIT", "1", SQL_C_BINARY, "01");
	Test("BIT", "0", SQL_C_BINARY, "00");
	Test("TINYINT", "231", SQL_C_BINARY, "E7");
	Test("SMALLINT", "4321", SQL_C_BINARY, big_endian ? "10E1" : "E110");
	Test("INT", "1234567", SQL_C_BINARY, big_endian ? "0012D687" : "87D61200");
	/* TODO some Sybase versions */
	if (db_is_microsoft() && strncmp(version, "08.00.", 6) == 0) {
		int old_result = result;

		Test("BIGINT", "123456789012345", SQL_C_BINARY, big_endian ? "00007048860DDF79" : "79DF0D8648700000");
		if (result && strcmp(sbuf, "13000179DF0D86487000000000000000000000") == 0) {
			fprintf(stderr, "Ignore previous error. You should configure TDS 8.0 for this!!!\n");
			if (!old_result)
				result = 0;
		}
	}

	Test("DECIMAL", "1234.5678", SQL_C_BINARY, "120001D3040000000000000000000000000000");
	Test("NUMERIC", "8765.4321", SQL_C_BINARY, "1200013D220000000000000000000000000000");

	Test("FLOAT", "1234.5678", SQL_C_BINARY, big_endian ? "40934A456D5CFAAD" : "ADFA5C6D454A9340");
	Test("REAL", "8765.4321", SQL_C_BINARY, big_endian ? "4608F5BA" : "BAF50846");

	Test("SMALLMONEY", "765.4321", SQL_C_BINARY, big_endian ? "0074CBB1" : "B1CB7400");
	Test("MONEY", "4321234.5678", SQL_C_BINARY, big_endian ? "0000000A0FA8114E" : "0A0000004E11A80F");

	/* behavior is different from MS ODBC */
	if (db_is_microsoft() && !driver_is_freetds()) {
		Test("NCHAR(7)", "donald", SQL_C_BINARY, "64006F006E0061006C0064002000");
		Test("NTEXT", "duck", SQL_C_BINARY, "6400750063006B00");
		Test("NVARCHAR(20)", "daffy", SQL_C_BINARY, "64006100660066007900");
	}

	if (db_is_microsoft())
		Test("UNIQUEIDENTIFIER", "0DDF3B64-E692-11D1-AB06-00AA00BDD685", SQL_C_BINARY,
		     big_endian ? "0DDF3B64E69211D1AB0600AA00BDD685" : "643BDF0D92E6D111AB0600AA00BDD685");

	Disconnect();

	if (!result)
		printf("Done successfully!\n");
	return result;
}
