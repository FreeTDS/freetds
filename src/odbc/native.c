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

#include "tds.h"
#include "tdsodbc.h"
#include "prepare_query.h"

static char software_version[] = "$Id: native.c,v 1.4 2002-11-08 15:57:42 freddy77 Exp $";
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
	char *s, *d, *p;
	int quoted = 0;
	char quote_char = 0;
	int nest_syntax = 0;

	/* list of bit, used as stack, is call ? FIXME limites size... */
	unsigned long is_calls = 0;

	if (stmt->prepared_query)
		s = stmt->prepared_query;
	else if (stmt->query)
		s = stmt->query;
	else
		return SQL_ERROR;

	/* we can do it because result string will be
	 * not bigger than source string */
	d = s;
	while (*s) {
		if (!quoted && (*s == '"' || *s == '\'')) {
			quoted = 1;
			quote_char = *s;
		} else if (quoted && *s == quote_char) {
			quoted = 0;
		}

		if (quoted) {
			*d++ = *s++;
		} else if (*s == '{') {
	char *pcall = strstr(s, "call ");

			++nest_syntax;
			is_calls <<= 1;
			if (!pcall || ((pcall - s) != 1 && (pcall - s) != 3)) {
				/* syntax is always in the form {type ...} */
				p = strchr(s, ' ');
				if (!p)
					break;
				s = p + 1;
			} else {
	int len;

				s++;
				len = pcall - s;
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
native_sql(const char *odbc_sql, char **out)
{
	char *d;
}

#endif
