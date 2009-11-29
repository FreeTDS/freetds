#include "common.h"
#include <assert.h>

/* Test various bind type */

/*
 * This test is useful to test odbc_tds2sql function
 * odbc_tds2sql have some particular cases:
 * (1) numeric -> binary  numeric is different in ODBC
 * (2) *       -> binary  dependent from libTDS representation and ODBC one
 * (3) binary  -> char    TODO
 * (4) date    -> char    different format
 * Also we have to check normal char and wide char
 */

static char software_version[] = "$Id: data.c,v 1.32 2009-11-29 20:06:28 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int result = 0;
static char sbuf[1024];

static int ignore_select_error = 0;

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, const char *expected)
{
	unsigned char out_buf[256];
	SQLLEN out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	SQLWCHAR *wp;
	int i;

	SQLFreeStmt(Statement, SQL_UNBIND);
	SQLFreeStmt(Statement, SQL_RESET_PARAMS);

	/* execute a select to get data as wire */
	sprintf(sbuf, "SELECT CONVERT(%s, '%s') AS data", type, value_to_convert);
	if (strncmp(value_to_convert, "0x", 2) == 0)
		sprintf(sbuf, "SELECT CONVERT(%s, %s) COLLATE Latin1_General_CI_AS AS data", type, value_to_convert);
	else if (strcmp(type, "SQL_VARIANT") == 0)
		sprintf(sbuf, "SELECT CONVERT(SQL_VARIANT, %s) AS data", value_to_convert);
	else if (strncmp(value_to_convert, "u&'", 3) == 0)
		sprintf(sbuf, "SELECT CONVERT(%s, %s) AS data", type, value_to_convert);
	if (ignore_select_error) {
		if (Command2(sbuf, "SENo") == SQL_ERROR) {
			ResetStatement();
			return;
		}
	} else {
		Command(sbuf);
	}
	ignore_select_error = 0;
	SQLBindCol(Statement, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	CHKFetch("S");
	CHKFetch("No");
	CHKMoreResults("No");

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
	case SQL_C_CHAR:
		out_buf[sizeof(out_buf) - 1] = 0;
		sprintf(sbuf,"%u %s", (unsigned int) strlen((char *) out_buf), out_buf);
		break;
	case SQL_C_WCHAR:
		assert(out_len >=0 && (out_len % sizeof(SQLWCHAR)) == 0);
		sprintf(sbuf, "%u ", (unsigned int) (out_len / sizeof(SQLWCHAR)));
		wp = (SQLWCHAR*) out_buf;
		for (i = 0; i < out_len / sizeof(SQLWCHAR); ++i)
			if ((unsigned int) wp[i] < 256)
				sprintf(strchr(sbuf, 0), "%c", (char) wp[i]);
			else
				sprintf(strchr(sbuf, 0), "\\u%04x", (unsigned int) wp[i]);
		break;
	case SQL_C_LONG:
		assert(out_len == sizeof(SQLINTEGER));
		sprintf(sbuf, "%ld", (long int) *((SQLINTEGER *) out_buf));
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

	Connect();

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;

	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, "38 0 1 7B");

	/* all binary results */
	/* cases (2) */
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
	if ((db_is_microsoft() && db_version_int() >= 0x08000000u)
	    || (!db_is_microsoft() && strncmp(db_version(), "15.00.", 6) >= 0)) {
		int old_result = result;

		Test("BIGINT", "123456789012345", SQL_C_BINARY, big_endian ? "00007048860DDF79" : "79DF0D8648700000");
		if (result && strcmp(sbuf, "13000179DF0D86487000000000000000000000") == 0) {
			fprintf(stderr, "Ignore previous error. You should configure TDS 8.0 for this!!!\n");
			if (!old_result)
				result = 0;
		}
	}

	Test("INT", "-123", SQL_C_CHAR, "4 -123");
	Test("INT", "78654", SQL_C_WCHAR, "5 78654");
	Test("VARCHAR(10)", "  51245  ", SQL_C_LONG, "51245");
	if (db_is_microsoft() && (strncmp(db_version(), "08.00.", 6) == 0 || strncmp(db_version(), "09.00.", 6) == 0)) {
		/* nvarchar without extended characters */
		Test("NVARCHAR(20)", "test", SQL_C_CHAR, "4 test");
		/* nvarchar with extended characters */
		/* don't test with MS which usually have a not compatible encoding */
		if (driver_is_freetds())
			Test("NVARCHAR(20)", "0x830068006900f200", SQL_C_CHAR, "4 \x83hi\xf2");

		Test("VARCHAR(20)", "test", SQL_C_WCHAR, "4 test");
		/* nvarchar with extended characters */
		Test("NVARCHAR(20)", "0x830068006900f200", SQL_C_WCHAR, "4 \x83hi\xf2");
		Test("NVARCHAR(20)", "0xA406A5FB", SQL_C_WCHAR, "2 \\u06a4\\ufba5");
		/* NVARCHAR -> SQL_C_LONG */
		Test("NVARCHAR(20)", "-24785  ", SQL_C_LONG, "-24785");
	}

	ignore_select_error = 1;
	Test("UNIVARCHAR(10)", "u&'\\06A4\\FBA5'", SQL_C_WCHAR, "2 \\u06a4\\ufba5");

	/* case (1) */
	Test("DECIMAL", "1234.5678", SQL_C_BINARY, "120001D3040000000000000000000000000000");
	Test("NUMERIC", "8765.4321", SQL_C_BINARY, "1200013D220000000000000000000000000000");

	Test("FLOAT", "1234.5678", SQL_C_BINARY, big_endian ? "40934A456D5CFAAD" : "ADFA5C6D454A9340");
	Test("REAL", "8765.4321", SQL_C_BINARY, big_endian ? "4608F5BA" : "BAF50846");

	Test("SMALLMONEY", "765.4321", SQL_C_BINARY, big_endian ? "0074CBB1" : "B1CB7400");
	Test("MONEY", "4321234.5678", SQL_C_BINARY, big_endian ? "0000000A0FA8114E" : "0A0000004E11A80F");

	/* behavior is different from MS ODBC */
	if (db_is_microsoft()) {
		Test("NCHAR(7)", "donald", SQL_C_BINARY, "64006F006E0061006C0064002000");
		Test("NTEXT", "duck", SQL_C_BINARY, "6400750063006B00");
		Test("NVARCHAR(20)", "daffy", SQL_C_BINARY, "64006100660066007900");
	}

	if (db_is_microsoft())
		Test("UNIQUEIDENTIFIER", "0DDF3B64-E692-11D1-AB06-00AA00BDD685", SQL_C_BINARY,
		     big_endian ? "0DDF3B64E69211D1AB0600AA00BDD685" : "643BDF0D92E6D111AB0600AA00BDD685");

	/* case (4) */
	Test("DATETIME", "2006-06-09 11:22:44", SQL_C_CHAR, "23 2006-06-09 11:22:44.000");
	Test("SMALLDATETIME", "2006-06-12 22:37:21", SQL_C_CHAR, "19 2006-06-12 22:37:00");
	Test("DATETIME", "2006-06-09 11:22:44", SQL_C_WCHAR, "23 2006-06-09 11:22:44.000");
	Test("SMALLDATETIME", "2006-06-12 22:37:21", SQL_C_WCHAR, "19 2006-06-12 22:37:00");

	if (db_is_microsoft() && db_version_int() >= 0x08000000u) {
		Test("SQL_VARIANT", "CAST('123' AS INT)", SQL_C_CHAR, "3 123");
		Test("SQL_VARIANT", "CAST('hello' AS CHAR(6))", SQL_C_CHAR, "6 hello ");
		Test("SQL_VARIANT", "CAST('ciao' AS VARCHAR(10))", SQL_C_CHAR, "4 ciao");
		Test("SQL_VARIANT", "CAST('foo' AS NVARCHAR(10))", SQL_C_CHAR, "3 foo");
		Test("SQL_VARIANT", "CAST('Super' AS NCHAR(8))", SQL_C_CHAR, "8 Super   ");
		Test("SQL_VARIANT", "CAST('321' AS VARBINARY(10))", SQL_C_CHAR, "6 333231");
		/* for some reasons MS ODBC seems to convert -123.4 to -123.40000000000001 */
		Test("SQL_VARIANT", "CAST('-123.5' AS FLOAT)", SQL_C_CHAR, "6 -123.5");
		Test("SQL_VARIANT", "CAST('-123.4' AS NUMERIC(10,2))", SQL_C_CHAR, "7 -123.40");
	}

	if (db_is_microsoft() && db_version_int() >= 0x09000000u) {
		Test("VARCHAR(MAX)", "goodbye!", SQL_C_CHAR, "8 goodbye!");
		Test("NVARCHAR(MAX)", "Micio mao", SQL_C_CHAR, "9 Micio mao");
		Test("VARBINARY(MAX)", "ciao", SQL_C_BINARY, "6369616F");
		Test("XML", "<a b=\"aaa\"><b>ciao</b>hi</a>", SQL_C_CHAR, "28 <a b=\"aaa\"><b>ciao</b>hi</a>");
	}

	Disconnect();

	if (!result)
		printf("Done successfully!\n");
	return result;
}
