/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>
#include <assert.h>

#include "tds.h"
#include "tdsodbc.h"
#include "prepare_query.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: native.c,v 1.11 2003-05-17 18:10:30 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

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
int
prepare_call(struct _hstmt *stmt)
{
	char *s, *d, *p, *buf;
	int nest_syntax = 0;

	/* list of bit, used as stack, is call ? FIXME limites size... */
	unsigned long is_calls = 0;

	if (stmt->prepared_query)
		buf = stmt->prepared_query;
	else if (stmt->query)
		buf = stmt->query;
	else
		return SQL_ERROR;

	/* we can do it because result string will be
	 * not bigger than source string */
	d = s = buf;
	while (*s) {
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

			while (isspace(*++s));
			pcall = s;
			if (*pcall == '?') {
				/* skip spaces after ? */
				while (isspace(*++pcall));
				if (*pcall == '=') {
					while (isspace(*++pcall));
				} else {
					/* avoid {?call ... syntax */
					pcall = s;
				}
			}
			if (strncasecmp(pcall, "call ", 5) != 0)
				pcall = NULL;

			++nest_syntax;
			is_calls <<= 1;
			if (!pcall) {
				/* assume syntax in the form {type ...} */
				p = strchr(s, ' ');
				if (!p)
					break;
				s = p + 1;
			} else {
				if (*s == '?')
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
	*d = '\0';

	/* now detect RPC */
	stmt->prepared_query_is_rpc = 0;
	s = buf;
	while (isspace(*s))
		++s;
	if (strncasecmp(s, "exec", 4) != 0 || !isspace(s[4]))
		return SQL_SUCCESS;
	s += 5;
	while (isspace(*s))
		++s;
	p = s;
	if (*s == '[') {
		s = (char *) tds_skip_quoted(s);
	} else {
		/* FIXME: stop at other characters ??? */
		while (!isspace(*s))
			++s;
	}
	--s;			/* trick, now s point to no blank */
	for (;;) {
		while (isspace(*++s));
		if (!*s)
			break;
		if (*s != '?')
			return SQL_SUCCESS;
		while (isspace(*++s));
		if (!*s)
			break;
		if (*s != ',')
			return SQL_SUCCESS;
	}
	stmt->prepared_query_is_rpc = 1;

	/* remove unneeded exec */
	assert(!*d);
	memmove(buf, p, d - p + 1);

	return SQL_SUCCESS;
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
