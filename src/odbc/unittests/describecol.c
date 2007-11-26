#include "common.h"
#include <ctype.h>

/*
 * SQLDescribeCol test for precision
 * test what say SQLDescribeCol about precision using some type
 */

static char software_version[] = "$Id: describecol.c,v 1.10 2007-11-26 06:25:11 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int g_result = 0;
static unsigned int line_num;

static void
fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	exit(1);
}

static int
get_int(const char *s)
{
	char *end;
	long l;

	if (!s)
		fatal("Line %u: NULL int\n", line_num);
	l = strtol(s, &end, 0);
	if (end[0])
		fatal("Line %u: Invalid int\n", line_num);
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
	TYPE(SQL_TYPE_DATE),
	TYPE(SQL_TYPE_TIME),
	TYPE(SQL_TYPE_TIMESTAMP)
#undef TYPE
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
	ATTR2(SQL_DESC_CONCISE_TYPE, SMALLINT, sql_types),
	ATTR2(SQL_DESC_TYPE, SMALLINT, sql_types)
#undef ATTR2
#undef ATTR
};

static const struct attribute *
lookup_attr(const char *name)
{
	int i;

	if (!name)
		fatal("Line %u: NULL attribute\n", line_num);
	for (i = 0; i < sizeof(attributes) / sizeof(attributes[0]); ++i)
		if (strcmp(attributes[i].name, name) == 0 || strcmp(attributes[i].name + 4, name) == 0)
			return &attributes[i];
	fatal("Line %u: attribute %s not found\n", line_num, name);
	return NULL;
}

#define SEP " \t\n"

#define ATTR_PARAMS const struct attribute *attr, int expected
typedef int (*get_attr_t) (ATTR_PARAMS);

static int
get_attr_ird(ATTR_PARAMS)
{
	SQLINTEGER i;
	SQLRETURN ret;

	if (attr->type == type_CHARP)
		fatal("Line %u: CHAR* check still not supported\n", line_num);
	i = 0xdeadbeef;
	ret = SQLColAttribute(Statement, 1, attr->value, NULL, SQL_IS_INTEGER, NULL, &i);
	if (!SQL_SUCCEEDED(ret))
		fatal("Line %u: failure not expected\n", line_num);
	return i;
}

static int
get_attr_ard(ATTR_PARAMS)
{
	SQLINTEGER i, ind;
	SQLSMALLINT si;
	SQLLEN li;
	SQLRETURN ret;
	SQLHDESC desc = SQL_NULL_HDESC;

	/* get ARD */
	SQLGetStmtAttr(Statement, SQL_ATTR_APP_ROW_DESC, &desc, sizeof(desc), &ind);

	ret = SQL_ERROR;
	switch (attr->type) {
	case type_INTEGER:
		i = 0xdeadbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & i, sizeof(SQLINTEGER), &ind);
		break;
	case type_SMALLINT:
		si = 0xbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & si, sizeof(SQLSMALLINT), &ind);
		i = si;
		break;
	case type_LEN:
		li = 0xdeadbeef;
		ret = SQLGetDescField(desc, 1, attr->value, (SQLPOINTER) & li, sizeof(SQLLEN), &ind);
		i = li;
		break;
	case type_CHARP:
		fatal("Line %u: CHAR* check still not supported\n", line_num);
		break;
	}
	if (!SQL_SUCCEEDED(ret))
		fatal("Line %u: failure not expected\n", line_num);
	return i;
}

/* do not retry any attribute just return expected value so to make caller happy */
static int
get_attr_none(ATTR_PARAMS)
{
	return expected;
}

int
main(int argc, char *argv[])
{
#define TEST_FILE "describecol.in"
	const char *in_file = FREETDS_SRCDIR "/" TEST_FILE;
	FILE *f;
	char buf[256];
	SQLINTEGER i;
	SQLLEN len;
	get_attr_t get_attr_p = get_attr_none;

	Connect();
	Command(Statement, "SET TEXTSIZE 4096");

	SQLBindCol(Statement, 1, SQL_C_SLONG, &i, sizeof(i), &len);

	f = fopen(in_file, "r");
	if (!f)
		fopen(TEST_FILE, "r");
	if (!f) {
		fprintf(stderr, "error opening test file\n");
		exit(1);
	}

	line_num = 0;
	while (fgets(buf, sizeof(buf), f)) {
		char *p = buf, *cmd;

		++line_num;

		while (isspace((unsigned char) *p))
			++p;
		cmd = strtok(p, SEP);

		/* skip comments */
		if (!cmd || cmd[0] == '#' || cmd[0] == 0 || cmd[0] == '\n')
			continue;

		if (strcmp(cmd, "odbc") == 0) {
			int odbc3 = get_int(strtok(NULL, SEP)) == 3 ? 1 : 0;

			if (use_odbc_version3 != odbc3) {
				use_odbc_version3 = odbc3;
				Disconnect();
				Connect();
				Command(Statement, "SET TEXTSIZE 4096");
				SQLBindCol(Statement, 1, SQL_C_SLONG, &i, sizeof(i), &len);
			}
		}

		/* select type */
		if (strcmp(cmd, "select") == 0) {
			const char *type = strtok(NULL, SEP);
			const char *value = strtok(NULL, SEP);
			char sql[sizeof(buf) + 40];

			SQLMoreResults(Statement);
			ResetStatement();

			sprintf(sql, "SELECT CONVERT(%s, %s) AS col", type, value);

			/* ignore error, we only need precision of known types */
			get_attr_p = get_attr_none;
			if (CommandWithResult(Statement, sql) != SQL_SUCCESS) {
				ResetStatement();
				SQLBindCol(Statement, 1, SQL_C_SLONG, &i, sizeof(i), &len);
				continue;
			}

			if (!SQL_SUCCEEDED(SQLFetch(Statement)))
				ODBC_REPORT_ERROR("Unable to fetch row");
			SQLBindCol(Statement, 1, SQL_C_SLONG, &i, sizeof(i), &len);
			get_attr_p = get_attr_ird;
		}

		/* set attribute */
		if (strcmp(cmd, "set") == 0) {
			const struct attribute *attr = lookup_attr(strtok(NULL, SEP));
			char *value = strtok(NULL, SEP);
			SQLHDESC desc;
			SQLRETURN ret;
			SQLINTEGER ind;

			if (!value)
				fatal("Line %u: value not defined\n", line_num);

			/* get ARD */
			SQLGetStmtAttr(Statement, SQL_ATTR_APP_ROW_DESC, &desc, sizeof(desc), &ind);

			ret = SQL_ERROR;
			switch (attr->type) {
			case type_INTEGER:
				ret = SQLSetDescField(desc, 1, attr->value, (SQLPOINTER) lookup(value, attr->lookup),
						      sizeof(SQLINTEGER));
				break;
			case type_SMALLINT:
				ret = SQLSetDescField(desc, 1, attr->value, (SQLPOINTER) lookup(value, attr->lookup),
						      sizeof(SQLSMALLINT));
				break;
			case type_LEN:
				ret = SQLSetDescField(desc, 1, attr->value, (SQLPOINTER) lookup(value, attr->lookup),
						      sizeof(SQLLEN));
				break;
			case type_CHARP:
				ret = SQLSetDescField(desc, 1, attr->value, (SQLPOINTER) value, SQL_NTS);
				break;
			}
			if (!SQL_SUCCEEDED(ret))
				fatal("Line %u: failure not expected setting ARD attribute\n", line_num);
			get_attr_p = get_attr_ard;
		}

		/* test attribute */
		if (strcmp(cmd, "attr") == 0) {
			const struct attribute *attr = lookup_attr(strtok(NULL, SEP));
			char *value = strtok(NULL, SEP);
			int i, expected = lookup(value, attr->lookup);

			if (!value)
				fatal("Line %u: value not defined\n", line_num);

			i = get_attr_p(attr, expected);
			if (i != expected) {
				g_result = 1;
				fprintf(stderr, "Line %u: invalid %s got %d expected %d\n", line_num, attr->name, i, expected);
			}
		}
	}

	fclose(f);
	Disconnect();

	printf("Done.\n");
	return g_result;
}
