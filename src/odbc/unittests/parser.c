/*
 * Test reading data with SQLBindCol
 */
#include "common.h"
#include <assert.h>
#include <ctype.h>
#include "parser.h"

enum
{
	MAX_BOOLS = 64,
	MAX_CONDITIONS = 32
};

typedef struct
{
	char *name;
	bool value;
} bool_t;

struct odbc_parser
{
	unsigned int line_num;

	bool_t bools[MAX_BOOLS];

	bool conds[MAX_CONDITIONS];
	unsigned cond_level;

	void *read_param;
	odbc_read_line_p read_func;

	char line_buf[1024];
};

unsigned int
odbc_line_num(odbc_parser *parser)
{
	return parser->line_num;
}

void
odbc_fatal(odbc_parser *parser, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (msg[0] == ':')
		fprintf(stderr, "Line %u", parser->line_num);
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
	if (!*s)
		return NULL;
	end = s + strcspn(s, SEP);
	*end = 0;
	*p = end + 1;
	return s;
}

static void
parse_cstr(odbc_parser *parser, char *s)
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
				odbc_fatal(parser, ": wrong string format\n");
			memcpy(hexbuf, ++s, 2);
			hexbuf[2] = 0;
			*d++ = (char) strtoul(hexbuf, NULL, 16);
			s += 2;
			break;
		default:
			odbc_fatal(parser, ": wrong string format\n");
		}
	}
	*d = 0;
}

const char *
odbc_get_str(odbc_parser *parser, char **p)
{
	char *s = *p, *end;

	s += strspn(s, SEP);
	if (!*s)
		odbc_fatal(parser, ": unable to get string\n");

	if (strncmp(s, "\"\"\"", 3) == 0) {
		s += 3;
		end = strstr(s, "\"\"\"");
		if (!end)
			odbc_fatal(parser, ": string not terminated\n");
		*end = 0;
		*p = end + 3;
	} else if (s[0] == '\"') {
		++s;
		end = strchr(s, '\"');
		if (!end)
			odbc_fatal(parser, ": string not terminated\n");
		*end = 0;
		parse_cstr(parser, s);
		*p = end + 1;
	} else {
		return odbc_get_tok(p);
	}
	return s;
}

void
odbc_set_bool(odbc_parser *parser, const char *name, bool value)
{
	unsigned n;

	for (n = 0; n < MAX_BOOLS && parser->bools[n].name; ++n)
		if (!strcmp(parser->bools[n].name, name)) {
			parser->bools[n].value = value;
			return;
		}

	if (n == MAX_BOOLS)
		odbc_fatal(parser, ": no more boolean variable free\n");
	parser->bools[n].name = strdup(name);
	if (!parser->bools[n].name)
		odbc_fatal(parser, ": out of memory\n");
	parser->bools[n].value = value;
}

static bool
get_bool(odbc_parser *parser, const char *name)
{
	unsigned n;

	if (!name)
		odbc_fatal(parser, ": boolean variable not provided\n");
	for (n = 0; n < MAX_BOOLS && parser->bools[n].name; ++n)
		if (!strcmp(parser->bools[n].name, name))
			return parser->bools[n].value;

	odbc_fatal(parser, ": boolean variable %s not found\n", name);
	return false;
}

/** initialize booleans, call after connection */
static void
init_bools(odbc_parser *parser)
{
	int big_endian = 1;

	if (((char *) &big_endian)[0] == 1)
		big_endian = 0;
	odbc_set_bool(parser, "bigendian", !!big_endian);

	odbc_set_bool(parser, "msdb", odbc_db_is_microsoft());
	odbc_set_bool(parser, "freetds", odbc_driver_is_freetds());
}

static void
clear_bools(odbc_parser *parser)
{
	unsigned n;

	for (n = 0; n < MAX_BOOLS && parser->bools[n].name; ++n) {
		free(parser->bools[n].name);
		parser->bools[n].name = NULL;
	}
}

static bool
pop_condition(odbc_parser *parser)
{
	if (parser->cond_level == 0)
		odbc_fatal(parser, ": no related if\n");
	return parser->conds[--parser->cond_level];
}

static void
push_condition(odbc_parser *parser, bool cond)
{
	if (parser->cond_level >= MAX_CONDITIONS)
		odbc_fatal(parser, ": too much nested conditions\n");
	parser->conds[parser->cond_level++] = cond;
}

static bool
get_not_cond(odbc_parser *parser, char **p)
{
	bool cond;
	const char *tok = odbc_get_tok(p);

	if (!tok)
		odbc_fatal(parser, ": wrong condition syntax\n");

	if (!strcmp(tok, "not"))
		cond = !get_bool(parser, odbc_get_tok(p));
	else
		cond = get_bool(parser, tok);

	return cond;
}

static bool
get_condition(odbc_parser *parser, char **p)
{
	bool cond1 = get_not_cond(parser, p), cond2;
	const char *tok;

	while ((tok = odbc_get_tok(p)) != NULL) {

		cond2 = get_not_cond(parser, p);

		if (!strcmp(tok, "or"))
			cond1 = cond1 || cond2;
		else if (!strcmp(tok, "and"))
			cond1 = cond1 && cond2;
		else
			odbc_fatal(parser, ": wrong condition syntax\n");
	}
	return cond1;
}

odbc_parser *
odbc_init_parser_func(odbc_read_line_p read_func, void *param)
{
	odbc_parser *parser;

	if (!read_func) {
		fprintf(stderr, "Missing reading function\n");
		exit(1);
	}

	parser = tds_new0(odbc_parser, 1);
	if (!parser) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	parser->read_param = param;
	parser->read_func = read_func;
	init_bools(parser);

	return parser;
}

static char *
read_file(void *param, char *s, size_t size)
{
	return fgets(s, size, (FILE *) param);
}

odbc_parser *
odbc_init_parser(FILE *f)
{
	return odbc_init_parser_func(read_file, f);
}

void
odbc_free_parser(odbc_parser *parser)
{
	clear_bools(parser);
	free(parser);
}

const char *
odbc_get_cmd_line(odbc_parser *parser, char **p_s, bool *cond)
{
	while (parser->read_func(parser->read_param, parser->line_buf, sizeof(parser->line_buf))) {
		char *p = parser->line_buf;
		const char *cmd;

		++parser->line_num;

		cmd = odbc_get_tok(&p);

		/* skip comments */
		if (!cmd || cmd[0] == '#' || cmd[0] == 0 || cmd[0] == '\n')
			continue;

		/* conditional statement */
		if (!strcmp(cmd, "else")) {
			bool c = pop_condition(parser);

			push_condition(parser, c);
			*cond = c && !*cond;
			continue;
		}
		if (!strcmp(cmd, "endif")) {
			*cond = pop_condition(parser);
			continue;
		}
		if (!strcmp(cmd, "if")) {
			push_condition(parser, *cond);
			if (*cond)
				*cond = get_condition(parser, &p);
			continue;
		}

		if (strcmp(cmd, "tds_version_cmp") == 0) {
			const char *bool_name = odbc_get_tok(&p);
			const char *cmp = odbc_get_tok(&p);
			const char *s_ver = odbc_get_tok(&p);
			int ver = odbc_tds_version();
			int expected;
			bool res;
			unsigned M, m;

			if (!cmp || !s_ver)
				odbc_fatal(parser, ": missing parameters\n");
			if (sscanf(s_ver, "%u.%u", &M, &m) != 2)
				odbc_fatal(parser, ": invalid version %s\n", s_ver);
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
				odbc_fatal(parser, ": invalid operator %s\n", cmp);

			if (*cond)
				odbc_set_bool(parser, bool_name, res);
			continue;
		}

		*p_s = p;
		return cmd;
	}
	return NULL;
}
