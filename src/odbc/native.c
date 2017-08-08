/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
 * Copyright (C) 2004, 2005 Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <ctype.h>
#include <assert.h>

#include <freetds/odbc.h>
#include <freetds/string.h>

#define TDS_ISSPACE(c) isspace((unsigned char) (c))
#define TDS_ISALPHA(c) isalpha((unsigned char) (c))

/*
 * Function transformation (from ODBC to Sybase)
 * String functions
 * ASCII(string) -> ASCII(string)
 * BIT_LENGTH(string) -> 8*OCTET_LENGTH(string)
 * CHAR_LENGTH(string_exp) -> CHAR_LENGTH(string_exp)
 * CHARACTER_LENGTH(string_exp) -> CHAR_LENGTH(string_exp)
 * CONCAT(string_exp1, string_exp2) -> string_exp1 + string_exp2
 * DIFFERENCE(string_exp1, string_exp2) -> DIFFERENCE(string_exp1, string_exp2)
 * INSERT(string_exp1, start, length, string_exp2) -> STUFF(sameparams)
 * LCASE(string_exp) -> LOWER(string)
 * LEFT(string_exp, count) -> SUBSTRING(string, 1, count)
 * LENGTH(string_exp) -> CHAR_LENGTH(RTRIM(string_exp))
 * LOCATE(string, string [,start]) -> CHARINDEX(string, string)
 * (SQLGetInfo should return third parameter not possible)
 * LTRIM(String) -> LTRIM(String)
 * OCTET_LENGTH(string_exp) -> OCTET_LENGTH(string_exp)
 * POSITION(character_exp IN character_exp) ???
 * REPEAT(string_exp, count) -> REPLICATE(same)
 * REPLACE(string_exp1, string_exp2, string_exp3) -> ??
 * RIGHT(string_exp, count) -> RIGHT(string_exp, count)
 * RTRIM(string_exp) -> RTRIM(string_exp)
 * SOUNDEX(string_exp) -> SOUNDEX(string_exp)
 * SPACE(count) (ODBC 2.0) -> SPACE(count) (ODBC 2.0)
 * SUBSTRING(string_exp, start, length) -> SUBSTRING(string_exp, start, length)
 * UCASE(string_exp) -> UPPER(string)
 *
 * Numeric
 * Nearly all function use same parameters, except:
 * ATAN2 -> ATN2
 * TRUNCATE -> ??
 */
static SQLRETURN
to_native(struct _hdbc *dbc, struct _hstmt *stmt, DSTR *str)
{
	char *d, *s;
	int nest_syntax = 0;
	char *buf = tds_dstr_buf(str);

	/* list of bit, used as stack, is call ? FIXME limites size... */
	unsigned long is_calls = 0;
	int server_scalar;

	assert(dbc);

	server_scalar = TDS_IS_MSSQL(dbc->tds_socket) && dbc->tds_socket->conn->product_version >= TDS_MS_VER(7, 0, 0);

	/*
	 * we can do it because result string will be
	 * not bigger than source string
	 */
	d = s = buf;
	while (*s) {
		if (*s == '-' || *s == '/') {
			size_t len_comment = tds_skip_comment(s) - s;

			memmove(d, s, len_comment);
			s += len_comment;
			d += len_comment;
			continue;
		}

		/* TODO: test syntax like "select 1 as [pi]]p)p{?=call]]]]o], 2" on mssql7+ */
		if (*s == '"' || *s == '\'' || *s == '[') {
			size_t len_quote = tds_skip_quoted(s) - s;

			memmove(d, s, len_quote);
			s += len_quote;
			d += len_quote;
			continue;
		}

		if (*s == '{') {
			char *pcall;

			while (TDS_ISSPACE(*++s))
				continue;
			pcall = s;
			/* FIXME if nest_syntax > 0 problems */
			if (server_scalar && strncasecmp(pcall, "fn ", 3) == 0) {
				*d++ = '{';
				continue;
			}
			if (*pcall == '?') {
				/* skip spaces after ? */
				while (TDS_ISSPACE(*++pcall))
					continue;
				if (*pcall == '=') {
					while (TDS_ISSPACE(*++pcall))
						continue;
				} else {
					/* avoid {?call ... syntax */
					pcall = s;
				}
			}
			if (strncasecmp(pcall, "call ", 5) != 0)
				pcall = NULL;

			if (stmt)
				stmt->prepared_query_is_rpc = 1;
			++nest_syntax;
			is_calls <<= 1;
			if (!pcall) {
				/* assume syntax in the form {type ...} */
				while (TDS_ISALPHA(*s))
					++s;
				while (TDS_ISSPACE(*s))
					++s;
			} else {
				if (*s == '?' && stmt)
					stmt->prepared_query_is_func = 1;
				memcpy(d, "exec ", 5);
				d += 5;
				s = pcall + 5;
				is_calls |= 1;
			}
		} else if (nest_syntax > 0) {
			/* do not copy close syntax */
			if (*s == '}') {
				--nest_syntax;
				is_calls >>= 1;
				++s;
				continue;
				/* convert parenthesis in call to spaces */
			} else if ((is_calls & 1) && (*s == '(' || *s == ')')) {
				*d++ = ' ';
				s++;
			} else {
				*d++ = *s++;
			}
		} else {
			*d++ = *s++;
		}
	}
	tds_dstr_setlen(str, d - buf);
	return SQL_SUCCESS;
}

const char *
parse_const_param(const char *s, TDS_SERVER_TYPE *type)
{
	char *end;

	/* binary */
	if (strncasecmp(s, "0x", 2) == 0) {
		s += 2;
		while (isxdigit(*s))
			++s;
		*type = SYBVARBINARY;
		return s;
	}

	/* string */
	if (*s == '\'') {
		*type = SYBVARCHAR;
		return tds_skip_quoted(s);
	}

	/* integer/float */
	if (isdigit(*s) || *s == '+' || *s == '-') {
		errno = 0;
		strtod(s, &end);
		if (end != s && strcspn(s, ".eE") < (size_t) (end-s) && errno == 0) {
			*type = SYBFLT8;
			return end;
		}
		errno = 0;
		/* FIXME success if long is 64bit */
		strtol(s, &end, 10);
		if (end != s && errno == 0) {
			*type = SYBINT4;
			return end;
		}
	}

	/* TODO date, time */

	return NULL;
}

const char *
odbc_skip_rpc_name(const char *s)
{
	for (;*s; ++s) {
		if (*s == '[') {
			/* handle [dbo].[name] and [master]..[name] syntax */
			s = tds_skip_quoted(s);
			if (*s != '.')
				break;
		} else if (TDS_ISSPACE(*s)) {
			/* FIXME: stop at other characters ??? */
			break;
		}
	}
	return s;
}

SQLRETURN
prepare_call(struct _hstmt * stmt)
{
	const char *s, *p, *param_start;
	char *buf;
	SQLRETURN rc;
	TDS_SERVER_TYPE type;

	if (tds_dstr_isempty(&stmt->query))
		return SQL_ERROR;

	if ((!tds_dstr_isempty(&stmt->attr.qn_msgtext) || !tds_dstr_isempty(&stmt->attr.qn_options)) && !IS_TDS72_PLUS(stmt->dbc->tds_socket->conn)) {
		odbc_errs_add(&stmt->errs, "HY000", "Feature is not supported by this server");
		return SQL_SUCCESS_WITH_INFO;
	}

	if ((rc = to_native(stmt->dbc, stmt, &stmt->query)) != SQL_SUCCESS)
		return rc;

	/* now detect RPC */
	if (stmt->prepared_query_is_rpc == 0)
		return SQL_SUCCESS;
	stmt->prepared_query_is_rpc = 0;

	s = buf = tds_dstr_buf(&stmt->query);
	while (TDS_ISSPACE(*s))
		++s;
	if (strncasecmp(s, "exec", 4) == 0) {
		if (TDS_ISSPACE(s[4]))
			s += 5;
		else if (strncasecmp(s, "execute", 7) == 0 && TDS_ISSPACE(s[7]))
			s += 8;
		else {
			stmt->prepared_query_is_func = 0;
			return SQL_SUCCESS;
		}
	}
	while (TDS_ISSPACE(*s))
		++s;
	p = s;
	s = (char *) odbc_skip_rpc_name(s);
	param_start = s;
	--s;			/* trick, now s point to no blank */
	for (;;) {
		while (TDS_ISSPACE(*++s))
			continue;
		if (!*s)
			break;
		switch (*s) {
		case '?':
			break;
		case ',':
			--s;
			break;
		default:
			if (!(s = parse_const_param(s, &type))) {
				stmt->prepared_query_is_func = 0;
				return SQL_SUCCESS;
			}
			--s;
			break;
		}
		while (TDS_ISSPACE(*++s))
			continue;
		if (!*s)
			break;
		if (*s != ',') {
			stmt->prepared_query_is_func = 0;
			return SQL_SUCCESS;
		}
	}
	stmt->prepared_query_is_rpc = 1;

	/* remove unneeded exec */
	s += strlen(s);
	memmove(buf, p, s - p);
	tds_dstr_setlen(&stmt->query, s - p);
	stmt->prepared_pos = buf + (param_start - p);

	return SQL_SUCCESS;
}

/* TODO handle output parameter and not terminated string */
SQLRETURN
native_sql(struct _hdbc * dbc, DSTR *s)
{
	return to_native(dbc, NULL, s);
}

/* function info */
struct func_info;
struct native_info;
typedef void (*special_fn) (struct native_info * ni, struct func_info * fi, char **params);

struct func_info
{
	const char *name;
	int num_param;
	const char *sql_name;
	special_fn special;
};

struct native_info
{
	char *d;
	int length;
};

#if 0				/* developing ... */

#define MAX_PARAMS 4

static const struct func_info funcs[] = {
	/* string functions */
	{"ASCII", 1},
	{"BIT_LENGTH", 1, "(8*OCTET_LENGTH", fn_parentheses},
	{"CHAR", 1},
	{"CHAR_LENGTH", 1},
	{"CHARACTER_LENGTH", 1, "CHAR_LENGTH"},
	{"CONCAT", 2, NULL, fn_concat},	/* a,b -> a+b */
	{"DIFFERENCE", 2},
	{"INSERT", 4, "STUFF"},
	{"LCASE", 1, "LOWER"},
	{"LEFT", 2, "SUBSTRING", fn_left},
	{"LENGTH", 1, "CHAR_LENGTH(RTRIM", fn_parentheses},
	{"LOCATE", 2, "CHARINDEX"},
/* (SQLGetInfo should return third parameter not possible) */
	{"LTRIM", 1},
	{"OCTET_LENGTH", 1},
/*	 POSITION(character_exp IN character_exp) */
	{"REPEAT", 2, "REPLICATE"},
/*	 REPLACE(string_exp1, string_exp2, string_exp3) */
	{"RIGHT", 2},
	{"RTRIM", 1},
	{"SOUNDEX", 1},
	{"SPACE", 1},
	{"SUBSTRING", 3},
	{"UCASE", 1, "UPPER"},

	/* numeric functions */
	{"ABS", 1},
	{"ACOS", 1},
	{"ASIN", 1},
	{"ATAN", 1},
	{"ATAN2", 2, "ATN2"},
	{"CEILING", 1},
	{"COS", 1},
	{"COT", 1},
	{"DEGREES", 1},
	{"EXP", 1},
	{"FLOOR", 1},
	{"LOG", 1},
	{"LOG10", 1},
	{"MOD", 2, NULL, fn_mod},	/* mod(a,b) -> ((a)%(b)) */
	{"PI", 0},
	{"POWER", 2},
	{"RADIANS", 1},
	{"RAND", -1, NULL, fn_rand},	/* accept 0 or 1 parameters */
	{"ROUND", 2},
	{"SIGN", 1},
	{"SIN", 1},
	{"SQRT", 1},
	{"TAN", 1},
/*	 TRUNCATE(numeric_exp, integer_exp) */

	/* system functions */
	{"DATABASE", 0, "DB_NAME"},
	{"IFNULL", 2, "ISNULL"},
	{"USER", 0, "USER_NAME"}

};

/**
 * Parse given sql and return converted sql
 */
int
odbc_native_sql(const char *odbc_sql, char **out)
{
	char *d;
}

#endif
