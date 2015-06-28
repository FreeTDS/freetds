/*
 * Test reading data with SQLBindCol
 */
#include "common.h"
#include <assert.h>
#include <ctype.h>
#include "parser.h"

unsigned int odbc_line_num;

void
odbc_fatal(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (msg[0] == ':')
		fprintf(stderr, "Line %u", odbc_line_num);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	exit(1);
}

#define SEP " \t\n"

const char *
odbc_get_tok(char **p)
{
	char *s = *p, *end;
	s += strspn(s, SEP);
	if (!*s) return NULL;
	end = s + strcspn(s, SEP);
	*end = 0;
	*p = end+1;
	return s;
}

static void
parse_cstr(char *s)
{
	char hexbuf[4];
	char *d = s;

	while (*s) {
		if (*s != '\\') {
			*d++ = *s++;
			continue;
		}

		switch (*++s) {
		case '\"':
			*d++ = *s++;
			break;
		case '\\':
			*d++ = *s++;
			break;
		case 'x':
			if (strlen(s) < 3)
				odbc_fatal(": wrong string format\n");
			memcpy(hexbuf, ++s, 2);
			hexbuf[2] = 0;
			*d++ = (char) strtoul(hexbuf, NULL, 16);
			s += 2;
			break;
		default:
			odbc_fatal(": wrong string format\n");
		}
	}
	*d = 0;
}

const char *
odbc_get_str(char **p)
{
	char *s = *p, *end;
	s += strspn(s, SEP);
	if (!*s) odbc_fatal(": unable to get string\n");

	if (strncmp(s, "\"\"\"", 3) == 0) {
		s += 3;
		end = strstr(s, "\"\"\"");
		if (!end) odbc_fatal(": string not terminated\n");
		*end = 0;
		*p = end+3;
	} else if (s[0] == '\"') {
		++s;
		end = strchr(s, '\"');
		if (!end) odbc_fatal(": string not terminated\n");
		*end = 0;
		parse_cstr(s);
		*p = end+1;
	} else {
		return odbc_get_tok(p);
	}
	return s;
}

enum { MAX_BOOLS = 64 };
typedef struct {
	char *name;
	int value;
} bool_t;
static bool_t bools[MAX_BOOLS];

void
odbc_set_bool(const char *name, int value)
{
	unsigned n;
	value = !!value;
	for (n = 0; n < MAX_BOOLS && bools[n].name; ++n)
		if (!strcmp(bools[n].name, name)) {
			bools[n]. value = value;
			return;
		}

	if (n == MAX_BOOLS)
		odbc_fatal(": no more boolean variable free\n");
	bools[n].name = strdup(name);
	if (!bools[n].name) odbc_fatal(": out of memory\n");
	bools[n].value = value;
}

static int
get_bool(const char *name)
{
	unsigned n;
	if (!name)
		odbc_fatal(": boolean variable not provided\n");
	for (n = 0; n < MAX_BOOLS && bools[n].name; ++n)
		if (!strcmp(bools[n].name, name))
			return bools[n]. value;

	odbc_fatal(": boolean variable %s not found\n", name);
	return 0;
}

/** initialize booleans, call after connection */
void
odbc_init_bools(void)
{
	int big_endian = 1;

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;
	odbc_set_bool("bigendian", big_endian);

	odbc_set_bool("msdb", odbc_db_is_microsoft());
	odbc_set_bool("freetds", odbc_driver_is_freetds());
}

void
odbc_clear_bools(void)
{
	unsigned n;
	for (n = 0; n < MAX_BOOLS && bools[n].name; ++n) {
		free(bools[n].name);
		bools[n].name = NULL;
	}
}

enum { MAX_CONDITIONS = 32 };
static char conds[MAX_CONDITIONS];
static unsigned cond_level = 0;

static int
pop_condition(void)
{
	if (cond_level == 0) odbc_fatal(": no related if\n");
	return conds[--cond_level];
}

static void
push_condition(int cond)
{
	if (cond != 0 && cond != 1) odbc_fatal(": invalid cond value %d\n", cond);
	if (cond_level >= MAX_CONDITIONS) odbc_fatal(": too much nested conditions\n");
	conds[cond_level++] = cond;
}

static int
get_not_cond(char **p)
{
	int cond;
	const char *tok = odbc_get_tok(p);
	if (!tok) odbc_fatal(": wrong condition syntax\n");

	if (!strcmp(tok, "not"))
		cond = !get_bool(odbc_get_tok(p));
	else
		cond = get_bool(tok);

	return cond;
}

static int
get_condition(char **p)
{
	int cond1 = get_not_cond(p), cond2;
	const char *tok;

	while ((tok=odbc_get_tok(p)) != NULL) {

		cond2 = get_not_cond(p);

		if (!strcmp(tok, "or"))
			cond1 = cond1 || cond2;
		else if (!strcmp(tok, "and"))
			cond1 = cond1 && cond2;
		else odbc_fatal(": wrong condition syntax\n");
	}
	return cond1;
}

static FILE *parse_file;
static char line_buf[1024];

void
odbc_init_parser(FILE *f)
{
	if (parse_file)
		odbc_fatal("parser file already setup\n");
	parse_file = f;
	odbc_line_num = 0;
	odbc_tds_version();
}

const char *
odbc_get_cmd_line(char **p_s, int *cond)
{
	while (fgets(line_buf, sizeof(line_buf), parse_file)) {
		char *p = line_buf;
		const char *cmd;

		++odbc_line_num;

		cmd = odbc_get_tok(&p);

		/* skip comments */
		if (!cmd || cmd[0] == '#' || cmd[0] == 0 || cmd[0] == '\n')
			continue;

		/* conditional statement */
		if (!strcmp(cmd, "else")) {
			int c = pop_condition();
			push_condition(c);
			*cond = c && !*cond;
			continue;
		}
		if (!strcmp(cmd, "endif")) {
			*cond = pop_condition();
			continue;
		}
		if (!strcmp(cmd, "if")) {
			push_condition(*cond);
			if (*cond)
				*cond = get_condition(&p);
			continue;
		}

		if (strcmp(cmd, "tds_version_cmp") == 0) {
			const char *bool_name = odbc_get_tok(&p);
			const char *cmp = odbc_get_tok(&p);
			const char *s_ver = odbc_get_tok(&p);
			int ver = odbc_tds_version();
			int expected;
			int res;
			unsigned M, m;

			if (!cmp || !s_ver)
				odbc_fatal(": missing parameters\n");
			if (sscanf(s_ver, "%u.%u", &M, &m) != 2)
				odbc_fatal(": invalid version %s\n", s_ver);
			expected = M * 0x100u + m;

			if (strcmp(cmp, ">") == 0)
				res = ver > expected;
			else if (strcmp(cmp, ">=") == 0)
				res = ver >= expected;
			else if (strcmp(cmp, "<") == 0)
				res = ver < expected;
			else if (strcmp(cmp, "<=") == 0)
				res = ver <= expected;
			else if (strcmp(cmp, "==") == 0)
				res = ver == expected;
			else if (strcmp(cmp, "!=") == 0)
				res = ver != expected;
			else
				odbc_fatal(": invalid operator %s\n", cmp);

			if (*cond)
				odbc_set_bool(bool_name, res);
			continue;
		}

		*p_s = p;
		return cmd;
	}
	return NULL;
}

