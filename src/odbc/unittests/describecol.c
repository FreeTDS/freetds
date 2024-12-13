#include "common.h"
#include <ctype.h>
#include "parser.h"
#include <odbcss.h>
#include <freetds/bool.h>

/*
 * SQLDescribeCol test for precision
 * test what say SQLDescribeCol about precision using some type
 */

static int g_result = 0;

static int
get_int(const char *s, odbc_parser *parser)
{
	char *end;
	long l;

	if (!s)
		odbc_fatal(parser, ": NULL int\n");
	l = strtol(s, &end, 0);
	if (end[0])
		odbc_fatal(parser, ": Invalid int\n");
	return (int) l;
}

struct lookup_int
{
	const char *name;
	int value;
};

static int
lookup(const char *name, const struct lookup_int *table, odbc_parser *parser)
{
	if (!table)
		return get_int(name, parser);

	for (; table->name; ++table)
		if (strcmp(table->name, name) == 0)
			return table->value;

	return get_int(name, parser);
}

static const char*
unlookup(long int value, const struct lookup_int *table)
{
	static char buf[32];

	sprintf(buf, "%ld", value);
	if (!table)
		return buf;

	for (; table->name; ++table)
		if (table->value == value)
			return table->name;

	return buf;
}


static struct lookup_int sql_types[] = {
#define TYPE(s) { #s, s }
	TYPE(SQL_CHAR),
	TYPE(SQL_VARCHAR),
	TYPE(SQL_LONGVARCHAR),
	TYPE(SQL_WCHAR),
	TYPE(SQL_WVARCHAR),
	TYPE(SQL_WLONGVARCHAR),
	TYPE(SQL_DECIMAL),
	TYPE(SQL_NUMERIC),
	TYPE(SQL_SMALLINT),
	TYPE(SQL_INTEGER),
	TYPE(SQL_REAL),
	TYPE(SQL_FLOAT),
	TYPE(SQL_DOUBLE),
	TYPE(SQL_BIT),
	TYPE(SQL_TINYINT),
	TYPE(SQL_BIGINT),
	TYPE(SQL_BINARY),
	TYPE(SQL_VARBINARY),
	TYPE(SQL_LONGVARBINARY),
	TYPE(SQL_DATE),
	TYPE(SQL_TIME),
	TYPE(SQL_TIMESTAMP),
	TYPE(SQL_TYPE_DATE),
	TYPE(SQL_TYPE_TIME),
	TYPE(SQL_TYPE_TIMESTAMP),
	TYPE(SQL_DATETIME),
	TYPE(SQL_SS_VARIANT),
	TYPE(SQL_SS_UDT),
	TYPE(SQL_SS_XML),
	TYPE(SQL_SS_TABLE),
	TYPE(SQL_SS_TIME2),
	TYPE(SQL_SS_TIMESTAMPOFFSET),
#undef TYPE
	{ NULL, 0 }
};

static struct lookup_int sql_bools[] = {
	{ "SQL_TRUE",  SQL_TRUE  },
	{ "SQL_FALSE", SQL_FALSE },
	{ NULL, 0 }
};

typedef enum
{
	type_INTEGER,
	type_SMALLINT,
	type_LEN,
	type_CHARP
} test_type_t;

struct attribute
{
	const char *name;
	int value;
	test_type_t type;
	const struct lookup_int *lookup;
};

static const struct attribute attributes[] = {
#define ATTR(s,t) { #s, s, type_##t, NULL }
#define ATTR2(s,t,l) { #s, s, type_##t, l }
	ATTR(SQL_COLUMN_LENGTH, INTEGER),
	ATTR(SQL_COLUMN_PRECISION, INTEGER),
	ATTR(SQL_COLUMN_SCALE, INTEGER),
	ATTR(SQL_DESC_LENGTH, LEN),
	ATTR(SQL_DESC_OCTET_LENGTH, LEN),
	ATTR(SQL_DESC_PRECISION, SMALLINT),
	ATTR(SQL_DESC_SCALE, SMALLINT),
	ATTR(SQL_DESC_DISPLAY_SIZE, INTEGER),
	ATTR(SQL_DESC_TYPE_NAME, CHARP),
	ATTR2(SQL_DESC_CONCISE_TYPE, SMALLINT, sql_types),
	ATTR2(SQL_DESC_TYPE, SMALLINT, sql_types),
	ATTR2(SQL_DESC_UNSIGNED, SMALLINT, sql_bools)
#undef ATTR2
#undef ATTR
};

static const struct attribute *
lookup_attr(const char *name, odbc_parser *parser)
{
	unsigned int i;

	if (!name)
		odbc_fatal(parser, ": NULL attribute\n");
	for (i = 0; i < TDS_VECTOR_SIZE(attributes); ++i)
		if (strcmp(attributes[i].name, name) == 0 || strcmp(attributes[i].name + 4, name) == 0)
			return &attributes[i];
	odbc_fatal(parser, ": attribute %s not found\n", name);
	return NULL;
}

#define ATTR_PARAMS \
	const struct attribute *attr TDS_UNUSED, \
	const char *expected_values[] TDS_UNUSED, \
	odbc_parser *parser TDS_UNUSED
typedef void (*check_attr_t) (ATTR_PARAMS);

static bool
is_contained(const char *s, const char *list[])
{
	for (;*list; ++list) {
		if (strcmp(s, *list) == 0)
			return true;
	}
	return false;
}

static bool
is_contained_lookup(SQLLEN i, const char *list[], const struct lookup_int *table, odbc_parser *parser)
{
	for (;*list; ++list) {
		if (i == lookup(*list, table, parser))
			return true;
	}
	return false;
}

static void
print_values(FILE* f, const char *list[])
{
	const char *sep = "[";
	for (;*list; ++list, sep = ", ")
		fprintf(f, "%s%s", sep, *list);
	fprintf(f, "]\n");
}

static void
check_attr_ird(ATTR_PARAMS)
{
	SQLLEN i;
	SQLRETURN ret;

	if (attr->type == type_CHARP) {
		char buf[256];
		SQLSMALLINT len;

		ret = SQLColAttribute(odbc_stmt, 1, attr->value, buf, sizeof(buf), &len, NULL);
		if (!SQL_SUCCEEDED(ret))
			odbc_fatal(parser, ": failure not expected\n");
		buf[sizeof(buf)-1] = 0;
		if (!is_contained(C((SQLTCHAR*) buf), expected_values)) {
			g_result = 1;
			fprintf(stderr, "Line %u: invalid %s got %s expected ", odbc_line_num(parser), attr->name, buf);
			print_values(stderr, expected_values);
		}
		return;
	}

	i = 0xdeadbeef;
	ret = SQLColAttribute(odbc_stmt, 1, attr->value, NULL, SQL_IS_INTEGER, NULL, &i);
	if (!SQL_SUCCEEDED(ret))
		odbc_fatal(parser, ": failure not expected\n");
	/* SQL_DESC_LENGTH is the same of SQLDescribeCol len */
	if (attr->value == SQL_DESC_LENGTH) {
		SQLSMALLINT scale, si;
		SQLULEN prec;
		CHKDescribeCol(1, NULL, 0, NULL, &si, &prec, &scale, &si, "S");
		if (i != prec)
			odbc_fatal(parser, ": attr %s SQLDescribeCol len %ld != SQLColAttribute len %ld\n",
				   attr->name, (long) prec, (long) i);
	}
	if (attr->value == SQL_DESC_SCALE) {
		SQLSMALLINT scale, si;
		SQLULEN prec;
		CHKDescribeCol(1, NULL, 0, NULL, &si, &prec, &scale, &si, "S");
		if (i != scale)
			odbc_fatal(parser, ": attr %s SQLDescribeCol scale %ld != SQLColAttribute len %ld\n",
				   attr->name, (long) scale, (long) i);
	}
	if (!is_contained_lookup(i, expected_values, attr->lookup, parser)) {
		g_result = 1;
		fprintf(stderr, "Line %u: invalid %s got %s expected ", odbc_line_num(parser), attr->name, unlookup(i, attr->lookup));
		print_values(stderr, expected_values);
	}
}

static void
check_attr_ard(ATTR_PARAMS)
{
	SQLINTEGER i, ind;
	SQLSMALLINT si;
	SQLLEN li;
	SQLRETURN ret;
	SQLHDESC desc = SQL_NULL_HDESC;
	char buf[256];

	/* get ARD */
	SQLGetStmtAttr(odbc_stmt, SQL_ATTR_APP_ROW_DESC, &desc, sizeof(desc), &ind);

	ret = SQL_ERROR;
	switch (attr->type) {
	case type_INTEGER:
		i = 0xdeadbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & i, sizeof(SQLINTEGER), &ind);
		li = i;
		break;
	case type_SMALLINT:
		si = 0xbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & si, sizeof(SQLSMALLINT), &ind);
		li = si;
		break;
	case type_LEN:
		li = 0xdeadbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & li, sizeof(SQLLEN), &ind);
		break;
	case type_CHARP:
		ret = SQLGetDescField(desc, 1, attr->value, buf, sizeof(buf), &ind);
		if (!SQL_SUCCEEDED(ret))
			odbc_fatal(parser, ": failure not expected\n");
		if (!is_contained(C((SQLTCHAR*) buf), expected_values)) {
			g_result = 1;
			fprintf(stderr, "Line %u: invalid %s got %s expected ", odbc_line_num(parser), attr->name, buf);
			print_values(stderr, expected_values);
		}
		return;
	}
	if (!SQL_SUCCEEDED(ret))
		odbc_fatal(parser, ": failure not expected\n");
	if (!is_contained_lookup(li, expected_values, attr->lookup, parser)) {
		g_result = 1;
		fprintf(stderr, "Line %u: invalid %s got %s expected ", odbc_line_num(parser), attr->name,
			unlookup(li, attr->lookup));
		print_values(stderr, expected_values);
	}
}

/* do not retry any attribute just return expected value so to make caller happy */
static void
check_attr_none(ATTR_PARAMS)
{
}

int
main(void)
{
	bool cond = true;
#define TEST_FILE "describecol.in"
	const char *in_file = FREETDS_SRCDIR "/" TEST_FILE;
	FILE *f;
	SQLINTEGER i;
	SQLLEN len;
	check_attr_t check_attr_p = check_attr_none;
	odbc_parser *parser;

	odbc_connect();
	odbc_command("SET TEXTSIZE 4096");

	SQLBindCol(odbc_stmt, 1, SQL_C_SLONG, &i, sizeof(i), &len);

	f = fopen(in_file, "r");
	if (!f)
		f = fopen(TEST_FILE, "r");
	if (!f) {
		fprintf(stderr, "error opening test file\n");
		exit(1);
	}

	/* cache version */
	odbc_tds_version();

	parser = odbc_init_parser(f);
	for (;;) {
		char *p;
		const char *cmd;

		cmd = odbc_get_cmd_line(parser, &p, &cond);
		if (!cmd)
			break;

		ODBC_FREE();

		if (strcmp(cmd, "odbc") == 0) {
			int odbc3 = get_int(odbc_get_tok(&p), parser) == 3 ? 1 : 0;

			if (!cond) continue;

			if (odbc_use_version3 != odbc3) {
				odbc_use_version3 = odbc3;
				odbc_disconnect();
				odbc_connect();
				odbc_command("SET TEXTSIZE 4096");
				SQLBindCol(odbc_stmt, 1, SQL_C_SLONG, &i, sizeof(i), &len);
			}
		}

		/* select type */
		if (strcmp(cmd, "select") == 0) {
			const char *type = odbc_get_str(parser, &p);
			const char *value = odbc_get_str(parser, &p);
			char sql[1024];

			if (!cond) continue;

			SQLMoreResults(odbc_stmt);
			odbc_reset_statement();

			snprintf(sql, sizeof(sql), "SELECT CONVERT(%s, %s) AS col", type, value);

			/* ignore error, we only need precision of known types */
			check_attr_p = check_attr_none;
			if (odbc_command_with_result(odbc_stmt, sql) != SQL_SUCCESS) {
				odbc_reset_statement();
				SQLBindCol(odbc_stmt, 1, SQL_C_SLONG, &i, sizeof(i), &len);
				continue;
			}

			CHKFetch("SI");
			SQLBindCol(odbc_stmt, 1, SQL_C_SLONG, &i, sizeof(i), &len);
			check_attr_p = check_attr_ird;
		}

		/* set attribute */
		if (strcmp(cmd, "set") == 0) {
			const struct attribute *attr = lookup_attr(odbc_get_tok(&p), parser);
			const char *value = odbc_get_str(parser, &p);
			SQLHDESC desc;
			SQLRETURN ret;
			SQLINTEGER ind;

			if (!value)
				odbc_fatal(parser, ": value not defined\n");

			if (!cond) continue;

			/* get ARD */
			SQLGetStmtAttr(odbc_stmt, SQL_ATTR_APP_ROW_DESC, &desc, sizeof(desc), &ind);

			ret = SQL_ERROR;
			switch (attr->type) {
			case type_INTEGER:
				ret = SQLSetDescField(desc, 1, attr->value, TDS_INT2PTR(lookup(value, attr->lookup, parser)),
						      sizeof(SQLINTEGER));
				break;
			case type_SMALLINT:
				ret = SQLSetDescField(desc, 1, attr->value, TDS_INT2PTR(lookup(value, attr->lookup, parser)),
						      sizeof(SQLSMALLINT));
				break;
			case type_LEN:
				ret = SQLSetDescField(desc, 1, attr->value, TDS_INT2PTR(lookup(value, attr->lookup, parser)),
						      sizeof(SQLLEN));
				break;
			case type_CHARP:
				ret = SQLSetDescField(desc, 1, attr->value, (SQLPOINTER) value, SQL_NTS);
				break;
			}
			if (!SQL_SUCCEEDED(ret))
				odbc_fatal(parser, ": failure not expected setting ARD attribute\n");
			check_attr_p = check_attr_ard;
		}

		/* test attribute */
		if (strcmp(cmd, "attr") == 0) {
			const struct attribute *attr = lookup_attr(odbc_get_tok(&p), parser);
			const char *expected[10];
			int i;

			expected[0] = odbc_get_str(parser, &p);

			if (!expected[0])
				odbc_fatal(parser, ": value not defined\n");

			if (!cond) continue;

			for (i = 1; ;++i) {
				if (i >= 10)
					odbc_fatal(parser, "Too many values in attribute\n");
				p += strspn(p, " \t");
				if (!*p) {
					expected[i] = NULL;
					break;
				}
				expected[i] = odbc_get_str(parser, &p);
			}
			check_attr_p(attr, expected, parser);
		}
	}

	fclose(f);
	odbc_free_parser(parser);
	odbc_disconnect();

	printf("Done.\n");
	return g_result;
}
