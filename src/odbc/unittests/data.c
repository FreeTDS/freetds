/*
 * Test reading data with SQLBindCol
 */
#include "common.h"
#include <assert.h>
#include <ctype.h>
#include "parser.h"

/*
 * This test is useful to test odbc_tds2sql function
 * odbc_tds2sql have some particular cases:
 * (1) numeric -> binary  numeric is different in ODBC
 * (2) *       -> binary  dependent from libTDS representation and ODBC one
 * (3) binary  -> char    TODO
 * (4) date    -> char    different format
 * Also we have to check normal char and wide char
 */

static int result = 0;

static int ignore_select_error = 0;
static int ignore_result = 0;

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLLEN out_len = 0;

	SQLFreeStmt(odbc_stmt, SQL_UNBIND);
	SQLFreeStmt(odbc_stmt, SQL_RESET_PARAMS);

	/* execute a select to get data as wire */
	sprintf(sbuf, "SELECT CONVERT(%s, '%s') AS data", type, value_to_convert);
	if (strncmp(value_to_convert, "0x", 2) == 0)
		sprintf(sbuf, "SELECT CONVERT(%s, %s) COLLATE Latin1_General_CI_AS AS data", type, value_to_convert);
	else if (strcmp(type, "SQL_VARIANT") == 0)
		sprintf(sbuf, "SELECT CONVERT(SQL_VARIANT, %s) AS data", value_to_convert);
	else if (strncmp(value_to_convert, "u&'", 3) == 0)
		sprintf(sbuf, "SELECT CONVERT(%s, %s) AS data", type, value_to_convert);
	if (ignore_select_error) {
		if (odbc_command2(sbuf, "SENo") == SQL_ERROR) {
			odbc_reset_statement();
			ignore_select_error = 0;
			ignore_result = 0;
			return;
		}
	} else {
		odbc_command(sbuf);
	}
	SQLBindCol(odbc_stmt, 1, out_c_type, out_buf, sizeof(out_buf), &out_len);
	CHKFetch("S");
	CHKFetch("No");
	CHKMoreResults("No");

	/* test results */
	odbc_c2string(sbuf, out_c_type, out_buf, out_len);

	if (!ignore_result && strcmp(sbuf, expected) != 0) {
		fprintf(stderr, "Wrong result\n  Got:      %s\n  Expected: %s\n", sbuf, expected);
		result = 1;
	}
	ignore_select_error = 0;
	ignore_result = 0;
}

static int
get_int(const char *s)
{
	char *end;
	long l;

	if (!s)
		odbc_fatal(": NULL int\n");
	l = strtol(s, &end, 0);
	if (end[0])
		odbc_fatal(": Invalid int\n");
	return (int) l;
}

struct lookup_int
{
	const char *name;
	int value;
};

static int
lookup(const char *name, const struct lookup_int *table)
{
	if (!table)
		return get_int(name);

	for (; table->name; ++table)
		if (strcmp(table->name, name) == 0)
			return table->value;

	return get_int(name);
}

static struct lookup_int sql_c_types[] = {
#define TYPE(s) { #s, s }
	TYPE(SQL_C_NUMERIC),
	TYPE(SQL_C_BINARY),
	TYPE(SQL_C_CHAR),
	TYPE(SQL_C_WCHAR),
	TYPE(SQL_C_LONG),
	TYPE(SQL_C_SBIGINT),
	TYPE(SQL_C_SHORT),
	TYPE(SQL_C_TIMESTAMP),
#undef TYPE
	{ NULL, 0 }
};

int
main(int argc, char *argv[])
{
	int cond = 1;

#define TEST_FILE "data.in"
	const char *in_file = FREETDS_SRCDIR "/" TEST_FILE;
	FILE *f;

	odbc_connect();

	odbc_init_bools();

	f = fopen(in_file, "r");
	if (!f)
		f = fopen(TEST_FILE, "r");
	if (!f) {
		fprintf(stderr, "error opening test file\n");
		exit(1);
	}

	odbc_init_parser(f);
	for (;;) {
		char *p;
		const char *cmd = odbc_get_cmd_line(&p, &cond);

		if (!cmd)
			break;

		/* select type */
		if (!strcmp(cmd, "select")) {
			const char *type = odbc_get_tok(&p);
			const char *value = odbc_get_str(&p);
			int c_type = lookup(odbc_get_tok(&p), sql_c_types);
			const char *expected = odbc_get_str(&p);

			if (!cond) continue;

			ignore_select_error = 1;
			Test(type, value, c_type, expected);
			continue;
		}
		/* select type setting condition */
		if (!strcmp(cmd, "select_cond")) {
			const char *bool_name = odbc_get_tok(&p);
			const char *type = odbc_get_tok(&p);
			const char *value = odbc_get_str(&p);
			int c_type = lookup(odbc_get_tok(&p), sql_c_types);
			const char *expected = odbc_get_str(&p);
			int save_result = result;

			if (!bool_name) odbc_fatal(": no condition name\n");
			if (!cond) continue;

			ignore_select_error = 1;
			ignore_result = 1;
			result = 0;
			Test(type, value, c_type, expected);
			odbc_set_bool(bool_name, result == 0);
			result = save_result;
			continue;
		}
		/* execute a sql command */
		if (!strcmp(cmd, "sql")) {
			const char *sql = odbc_get_str(&p);

			if (!cond) continue;

			odbc_command(sql);
			continue;
		}
		if (!strcmp(cmd, "sql_cond")) {
			const char *bool_name = odbc_get_tok(&p);
			const char *sql = odbc_get_str(&p);

			if (!cond) continue;

			odbc_set_bool(bool_name, odbc_command2(sql, "SENo") != SQL_ERROR);
			continue;
		}
		odbc_fatal(": unknown command\n");
	}
	odbc_clear_bools();
	fclose(f);

	printf("\n");

	odbc_disconnect();

	if (!result)
		printf("Done successfully!\n");
	return result;
}
