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

#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsodbc.h"
#include "prepare_query.h"
#include "convert_sql2string.h"
#include "odbc_util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: prepare_query.c,v 1.32 2003-08-29 15:47:56 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int
_get_sql_textsize(struct _sql_param_info *param, SQLINTEGER sql_len)
{
	int len;

	switch (param->ipd_sql_desc_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_BIT:
		len = sql_len;
		break;
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		/* allocate space for hex encoding "0x" prefix and terminating NULL */
		len = 2 * sql_len + 3;
		break;
	case SQL_DATE:
		len = 15;
		break;
	case SQL_TIME:
		len = 6;
		break;
	case SQL_TIMESTAMP:
		len = 22;
		break;
	case SQL_NUMERIC:
	case SQL_DECIMAL:
		len = 34;
		break;
	case SQL_INTEGER:
		len = 11;
		break;
	case SQL_SMALLINT:
		len = 6;
		break;
	case SQL_REAL:
		len = 25;
		break;
	case SQL_FLOAT:
	case SQL_DOUBLE:
		len = 54;
		break;
	case SQL_BIGINT:
		len = 20;
		break;
	case SQL_TINYINT:
		len = 4;
		break;
	default:
		len = -1;
		break;
	}

	return len;
}


static int
_get_param_textsize(TDS_STMT * stmt, struct _sql_param_info *param)
{
	int len = 0;
	SQLINTEGER sql_len = 0;

	if (param->apd_sql_desc_indicator_ptr && *param->apd_sql_desc_indicator_ptr == SQL_NULL_DATA)
		sql_len = SQL_NULL_DATA;
	else if (param->apd_sql_desc_octet_length_ptr)
		sql_len = *param->apd_sql_desc_octet_length_ptr;

	switch (sql_len) {
	case SQL_NTS:
		len = strlen(param->apd_sql_desc_data_ptr) + 1;
		break;
	case SQL_NULL_DATA:
		len = 4;
		break;
	case SQL_DEFAULT_PARAM:
	case SQL_DATA_AT_EXEC:
		/* I don't know what to do */
		odbc_errs_add(&stmt->errs, "HYC00", "SQL_DEFAULT_PARAM and SQL_DATA_AT_EXEC not supported", NULL);
		len = -1;
		break;
	default:
		len = sql_len;
		if (0 > len)
			len = SQL_LEN_DATA_AT_EXEC(len);
		else
			len = _get_sql_textsize(param, sql_len);
	}

	return len;
}

static int
_calculate_params_size(TDS_STMT * stmt)
{
	int i;
	int len = 0;
	int l;
	struct _sql_param_info *param;

	for (i = stmt->param_count; i > 0; --i) {
		param = odbc_find_param(stmt, i);
		if (!param)
			return -1;
		l = _get_param_textsize(stmt, param);
		if (l < 0)
			return -1;
		len += l;
	}

	return len;
}

static int
_need_comma(struct _sql_param_info *param)
{
	if (SQL_NULL_DATA == param->ipd_sql_desc_parameter_type)
		return 0;

	switch (param->ipd_sql_desc_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_DATE:
	case SQL_TIME:
	case SQL_TIMESTAMP:
		return 1;
	}

	return 0;
}

static int
_get_len_data_at_exec(SQLINTEGER sql_len)
{
	int len = 0;

	switch (sql_len) {
	case SQL_NTS:
	case SQL_NULL_DATA:
	case SQL_DEFAULT_PARAM:
	case SQL_DATA_AT_EXEC:
		len = -1;
		break;
	default:
		len = sql_len;
		if (0 > len)
			len = SQL_LEN_DATA_AT_EXEC(len);
		else
			len = -1;
	}

	return len;
}

static const char comma = '\'';

static char *
_get_last_comma(char *s, int len)
{
	s += len - 1;
	for (; len > 0; --len, --s)
		if (comma == *s)
			return s;

	return 0;
}

static int
_fix_commas(char *s, int s_len)
{
	int commas_cnt = 0;
	int len;
	int ret;
	char *pcomma;
	char *buf_beg;
	char *buf_end;

	buf_beg = s;
	buf_end = s + s_len - 1;
	len = s_len;
	while ((pcomma = (char *) memchr(buf_beg, comma, len))) {
		++commas_cnt;
		buf_beg = pcomma + 1;
		len = buf_end - pcomma;
	}
	ret = s_len + commas_cnt;


	buf_end = s + s_len;
	for (len = s_len; (pcomma = _get_last_comma(s, len)); len = pcomma - s) {
		memmove(pcomma + commas_cnt, pcomma, buf_end - pcomma);
		buf_end = pcomma;
		--commas_cnt;
		*(pcomma + commas_cnt) = comma;
	}

	return ret;
}

static int
parse_prepared_query(struct _hstmt *stmt, int start)
{
	char *s, *d;
	int param_num;
	struct _sql_param_info *param;
	TDSLOCALE *locale;
	TDSCONTEXT *context;
	int len;
	int need_comma;
	SQLINTEGER sql_len;

	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;

	if (start) {
		s = stmt->prepared_query;
		d = stmt->query;
		param_num = stmt->prepared_query_is_func ? 1 : 0;
	} else {
		/* load prepared_query parameters from stmt */
		s = stmt->prepared_query_s;
		d = stmt->prepared_query_d;
		param_num = stmt->prepared_query_param_num;
	}

	while (*s) {
		if (*s == '"' || *s == '\'' || *s == '[') {
			size_t len_quote = tds_skip_quoted(s) - s;

			memmove(d, s, len_quote);
			s += len_quote;
			d += len_quote;
			continue;
		}

		if (*s == '?') {
			param_num++;

			param = odbc_find_param(stmt, param_num);
			if (!param)
				return SQL_ERROR;

			need_comma = _need_comma(param);
			/* printf("ctype is %d %d %d\n",param->ipd_sql_desc_parameter_type, 
			 * param->apd_sql_desc_type, param->ipd_sql_desc_type); */

			if (need_comma)
				*d++ = comma;

			if (param->apd_sql_desc_type == SQL_C_BINARY) {
				memcpy(d, "0x", 2);
				d += 2;
			}

			sql_len = 0;
			if (param->apd_sql_desc_indicator_ptr && *param->apd_sql_desc_indicator_ptr == SQL_NULL_DATA)
				sql_len = SQL_NULL_DATA;
			else if (param->apd_sql_desc_octet_length_ptr)
				sql_len = *param->apd_sql_desc_octet_length_ptr;

			if (_get_len_data_at_exec(sql_len) > 0) {
				/* save prepared_query parameters to stmt */
				stmt->prepared_query_s = s;
				stmt->prepared_query_d = d;
				stmt->prepared_query_param_num = param_num;
				stmt->prepared_query_need_bytes = _get_len_data_at_exec(sql_len);

				/* stop parsing and ask for a data */
				return SQL_NEED_DATA;
			}

			len = convert_sql2string(context, param->apd_sql_desc_type, param->apd_sql_desc_data_ptr, sql_len, d, -1);
			if (TDS_FAIL == len)
				return SQL_ERROR;

			if (need_comma)
				len = _fix_commas(d, len);

			d += len;
			if (need_comma)
				*d++ = comma;
			s++;
		} else {
			*d++ = *s++;
		}
	}
	*d = '\0';
	/* reset prepared_query parameters in stmt
	 * to prevent wrong calls to this function */
	stmt->prepared_query_s = 0;

	return SQL_SUCCESS;
}

int
start_parse_prepared_query(struct _hstmt *stmt)
{
	int len;

	if (!stmt->prepared_query)
		return SQL_ERROR;

	len = _calculate_params_size(stmt);
	if (0 > len)
		return SQL_ERROR;

	len = strlen(stmt->prepared_query) + 1 + stmt->param_count * 2	/* reserve space for '' */
		+ len		/* reserve space for parameters */
		+ len / 2;	/* reserve space for ' inside strings */

	if (stmt->query)
		free(stmt->query);
	stmt->query = (char *) malloc(len + 1);
	if (!stmt->query)
		return SQL_ERROR;

	/* set prepared_query parameters in stmt */
	return parse_prepared_query(stmt, 1);
}

int
continue_parse_prepared_query(struct _hstmt *stmt, SQLPOINTER DataPtr, SQLINTEGER StrLen_or_Ind)
{
	char *d;
	struct _sql_param_info *param;
	TDSLOCALE *locale;
	TDSCONTEXT *context;
	int len;
	int need_bytes;

	if (!stmt->prepared_query)
		return SQL_ERROR;

	if (!stmt->prepared_query_s)
		return SQL_ERROR;

	if (stmt->prepared_query_need_bytes <= 0)
		return SQL_ERROR;

	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;
	param = odbc_find_param(stmt, stmt->prepared_query_param_num);
	if (!param)
		return SQL_ERROR;

	/* load prepared_query parameters from stmt */
	d = stmt->prepared_query_d;
	need_bytes = stmt->prepared_query_need_bytes;

	if (SQL_NTS == StrLen_or_Ind)
		StrLen_or_Ind = strlen((char *) DataPtr);
	else if (SQL_DEFAULT_PARAM == StrLen_or_Ind)
		/* FIXME: I don't know what to do */
		return SQL_ERROR;

	if (StrLen_or_Ind > need_bytes && SQL_NULL_DATA != StrLen_or_Ind)
		StrLen_or_Ind = need_bytes;

	/* put parameter into query */
	len = convert_sql2string(context, param->apd_sql_desc_type, (const TDS_CHAR *) DataPtr, StrLen_or_Ind, d, -1);
	if (TDS_FAIL == len)
		return SQL_ERROR;

	if (_need_comma(param))
		len = _fix_commas(d, len);

	d += len;

	need_bytes -= StrLen_or_Ind;
	if (StrLen_or_Ind > 0 && need_bytes > 0) {
		/* set prepared_query parameters in stmt */
		stmt->prepared_query_d = d;
		stmt->prepared_query_need_bytes = need_bytes;

		/* stop parsing and ask more data */
		return SQL_NEED_DATA;
	}

	/* continue parse prepared query */
	if (_need_comma(param))
		*d++ = comma;

	/* set prepared_query parameters in stmt */
	stmt->prepared_query_s++;
	stmt->prepared_query_d = d;
	stmt->prepared_query_need_bytes = 0;

	/* continue parsing */
	return parse_prepared_query(stmt, 0);
}
