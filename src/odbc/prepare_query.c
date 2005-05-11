/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#include <ctype.h>

#include "tds.h"
#include "tdsodbc.h"
#include "prepare_query.h"
#if 0
#include "convert_sql2string.h"
#endif
#include "odbc_util.h"
#include "sql2tds.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static const char software_version[] = "$Id: prepare_query.c,v 1.47 2005-05-11 12:03:28 freddy77 Exp $";
static const void *const no_unused_var_warn[] = { software_version, no_unused_var_warn };

#if 0
static int
_get_sql_textsize(struct _drecord *drec_ipd, SQLINTEGER sql_len)
{
	int len;

	switch (drec_ipd->sql_desc_concise_type) {
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
	case SQL_TYPE_TIMESTAMP:
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
	case SQL_GUID:
		len = 36;
		break;
	default:
		len = -1;
		break;
	}

	return len;
}


static int
_get_param_textsize(TDS_STMT * stmt, struct _drecord *drec_ipd, struct _drecord *drec_apd)
{
	int len = 0;
	SQLINTEGER sql_len = odbc_get_param_len(stmt->dbc->tds_socket, drec_apd, drec_ipd);

	switch (sql_len) {
	case SQL_NTS:
		len = strlen(drec_apd->sql_desc_data_ptr) + 1;
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
		len = _get_sql_textsize(drec_ipd, len);
	}

	return len;
}

static int
_calculate_params_size(TDS_STMT * stmt)
{
	int i;
	int len = 0;
	int l;

	for (i = stmt->param_count; i > 0; --i) {
		if (i > stmt->apd->header.sql_desc_count || i > stmt->ipd->header.sql_desc_count)
			return -1;
		l = _get_param_textsize(stmt, &stmt->ipd->records[i - 1], &stmt->apd->records[i - 1]);
		if (l < 0)
			return -1;
		len += l;
	}

	return len;
}

static int
_need_comma(struct _drecord *drec_ipd)
{
	if (SQL_NULL_DATA == drec_ipd->sql_desc_parameter_type)
		return 0;

	switch (drec_ipd->sql_desc_concise_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_DATE:
	case SQL_TIME:
	case SQL_TIMESTAMP:
	case SQL_TYPE_DATE:
	case SQL_TYPE_TIME:
	case SQL_TYPE_TIMESTAMP:
	case SQL_GUID:
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
	struct _drecord *drec_ipd, *drec_apd;
	TDSLOCALE *locale;
	TDSCONTEXT *context;
	int len;
	int need_comma;
	SQLINTEGER sql_len;

	context = stmt->dbc->env->tds_ctx;
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

			if (param_num > stmt->apd->header.sql_desc_count || param_num > stmt->ipd->header.sql_desc_count)
				return SQL_ERROR;
			drec_ipd = &stmt->ipd->records[param_num - 1];
			drec_apd = &stmt->apd->records[param_num - 1];

			need_comma = _need_comma(drec_ipd);
			/* printf("ctype is %d %d %d\n",param->ipd_sql_desc_parameter_type, 
			 * param->apd_sql_desc_concise_type, param->ipd_sql_desc_concise_type); */

			if (need_comma)
				*d++ = comma;

			if (drec_apd->sql_desc_concise_type == SQL_C_BINARY) {
				memcpy(d, "0x", 2);
				d += 2;
			}

			sql_len = odbc_get_param_len(stmt->dbc->tds_socket, drec_apd, drec_ipd);
			if (_get_len_data_at_exec(sql_len) > 0) {
				/* save prepared_query parameters to stmt */
				stmt->prepared_query_s = s;
				stmt->prepared_query_d = d;
				stmt->prepared_query_param_num = param_num;
				stmt->prepared_query_need_bytes = _get_len_data_at_exec(sql_len);

				/* stop parsing and ask for a data */
				return SQL_NEED_DATA;
			}

			len = convert_sql2string(context, drec_apd->sql_desc_concise_type, drec_apd->sql_desc_data_ptr, sql_len, d,
						 -1);
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
continue_parse_prepared_query(struct _hstmt *stmt, SQLPOINTER DataPtr, SQLLEN StrLen_or_Ind)
{
	char *d;
	struct _drecord *drec_apd, *drec_ipd;
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

	context = stmt->dbc->env->tds_ctx;
	locale = context->locale;
	if (stmt->prepared_query_param_num > stmt->apd->header.sql_desc_count
	    || stmt->prepared_query_param_num > stmt->ipd->header.sql_desc_count)
		return SQL_ERROR;
	drec_apd = &stmt->apd->records[stmt->prepared_query_param_num - 1];
	drec_ipd = &stmt->ipd->records[stmt->prepared_query_param_num - 1];

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
	len = convert_sql2string(context, drec_apd->sql_desc_concise_type, (const TDS_CHAR *) DataPtr, StrLen_or_Ind, d, -1);
	if (TDS_FAIL == len)
		return SQL_ERROR;

	if (_need_comma(drec_ipd))
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
	if (_need_comma(drec_ipd))
		*d++ = comma;

	/* set prepared_query parameters in stmt */
	stmt->prepared_query_s++;
	stmt->prepared_query_d = d;
	stmt->prepared_query_need_bytes = 0;

	/* continue parsing */
	return parse_prepared_query(stmt, 0);
}
#endif

#define TDS_ISSPACE(c) isspace((unsigned char) (c))

static int
prepared_rpc(struct _hstmt *stmt, int compute_row)
{
	int nparam = stmt->params ? stmt->params->num_cols : 0;
	const char *p = stmt->prepared_pos - 1;

	for (;;) {
		TDSPARAMINFO *temp_params;
		TDSCOLUMN *curcol;
		const char *start;

		while (TDS_ISSPACE(*++p));
		if (!*p)
			return SQL_SUCCESS;

		/* we have certainly a parameter */
		if (!(temp_params = tds_alloc_param_result(stmt->params))) {
			odbc_errs_add(&stmt->errs, "HY001", NULL, NULL);
			return SQL_ERROR;
		}
		stmt->params = temp_params;
		curcol = temp_params->columns[nparam];

		switch (*p) {
		case ',':
			tds_set_param_type(stmt->dbc->tds_socket, curcol, SYBVOID);
			curcol->column_size = curcol->column_cur_size = 0;
			if (compute_row)
				if (!tds_alloc_param_row(temp_params, curcol))
					return SQL_ERROR;
			--p;
			break;
		default:
			/* add next parameter to list */
			start = p;

			if (!(p = skip_const_param(p)))
				return SQL_ERROR;
			tds_set_param_type(stmt->dbc->tds_socket, curcol, SYBVARCHAR);
			curcol->column_size = p - start;
			/* TODO support other type other than VARCHAR, do not strip escape in prepare_call */
			if (compute_row) {
				char *dest;

				if (!tds_alloc_param_row(temp_params, curcol))
					return SQL_ERROR;
				dest = (char *) &temp_params->current_row[curcol->column_offset];
				if (*start != '\'') {
					memcpy(dest, start, p - start);
					curcol->column_cur_size = p - start;
				} else {
					++start;
					for (;;) {
						if (*start == '\'')
							++start;
						if (start >= p)
							break;
						*dest++ = *start++;
					}
					curcol->column_cur_size =
						dest - (char *) (&temp_params->current_row[curcol->column_offset]);
				}
			}
			--p;
			break;
		case '?':
			/* find binded parameter */
			if (stmt->param_num > stmt->apd->header.sql_desc_count
			    || stmt->param_num > stmt->ipd->header.sql_desc_count) {
				/* TODO set error */
				return SQL_ERROR;
			}

			switch (sql2tds
				(stmt->dbc, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
				 stmt->params, nparam, compute_row)) {
			case SQL_ERROR:
				return SQL_ERROR;
			case SQL_NEED_DATA:
				return SQL_NEED_DATA;
			}
			++stmt->param_num;
			break;
		}
		++nparam;

		while (TDS_ISSPACE(*++p));
		if (!*p || *p != ',')
			return SQL_SUCCESS;
		stmt->prepared_pos = (char *) p + 1;
	}
}

int
parse_prepared_query(struct _hstmt *stmt, int compute_row)
{
	/* try setting this parameter */
	TDSPARAMINFO *temp_params;
	int nparam = stmt->params ? stmt->params->num_cols : 0;

	if (stmt->prepared_pos)
		return prepared_rpc(stmt, compute_row);

	for (; stmt->param_num <= stmt->param_count; ++nparam, ++stmt->param_num) {
		/* find binded parameter */
		if (stmt->param_num > stmt->apd->header.sql_desc_count || stmt->param_num > stmt->ipd->header.sql_desc_count) {
			/* TODO set error */
			return SQL_ERROR;
		}

		/* add a column to parameters */
		if (!(temp_params = tds_alloc_param_result(stmt->params))) {
			odbc_errs_add(&stmt->errs, "HY001", NULL, NULL);
			return SQL_ERROR;
		}
		stmt->params = temp_params;

		switch (sql2tds
			(stmt->dbc, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
			 stmt->params, nparam, compute_row)) {
		case SQL_ERROR:
			return SQL_ERROR;
		case SQL_NEED_DATA:
			return SQL_NEED_DATA;
		}
	}
	return SQL_SUCCESS;
}

int
start_parse_prepared_query(struct _hstmt *stmt, int compute_row)
{
	/* TODO should be NULL already ?? */
	tds_free_param_results(stmt->params);
	stmt->params = NULL;
	stmt->param_num = 0;

	if (!stmt->param_count)
		return SQL_SUCCESS;
	stmt->param_num = stmt->prepared_query_is_func ? 2 : 1;
	return parse_prepared_query(stmt, compute_row);
}

int
continue_parse_prepared_query(struct _hstmt *stmt, SQLPOINTER DataPtr, SQLLEN StrLen_or_Ind)
{
	struct _drecord *drec_apd, *drec_ipd;
	SQLLEN len;
	int need_bytes;
	TDSCOLUMN *curcol;
	TDSBLOB *blob;

	if (!stmt->params)
		return SQL_ERROR;

	if (stmt->param_num > stmt->apd->header.sql_desc_count || stmt->param_num > stmt->ipd->header.sql_desc_count)
		return SQL_ERROR;
	drec_apd = &stmt->apd->records[stmt->param_num - 1];
	drec_ipd = &stmt->ipd->records[stmt->param_num - 1];

	curcol = stmt->params->columns[stmt->param_num - (stmt->prepared_query_is_func ? 2 : 1)];
	blob = NULL;
	if (is_blob_type(curcol->column_type))
		blob = (TDSBLOB *) (stmt->params->current_row + curcol->column_offset);
	assert(curcol->column_cur_size <= curcol->column_size);
	need_bytes = curcol->column_size - curcol->column_cur_size;

	if (SQL_NTS == StrLen_or_Ind)
		len = strlen((char *) DataPtr);
	else if (SQL_DEFAULT_PARAM == StrLen_or_Ind || StrLen_or_Ind < 0)
		/* FIXME: I don't know what to do */
		return SQL_ERROR;
	else
		len = StrLen_or_Ind;

	if (!blob && len > need_bytes)
		len = need_bytes;

	/* copy to destination */
	if (blob) {
		TDS_CHAR *p;

		if (blob->textvalue)
			p = (TDS_CHAR *) realloc(blob->textvalue, len + curcol->column_cur_size);
		else {
			assert(curcol->column_cur_size == 0);
			p = (TDS_CHAR *) malloc(len);
		}
		if (!p)
			return SQL_ERROR;
		blob->textvalue = p;
		memcpy(blob->textvalue + curcol->column_cur_size, DataPtr, len);
	} else {
		memcpy(stmt->params->current_row + curcol->column_offset + curcol->column_cur_size, DataPtr, len);
	}
	curcol->column_cur_size += len;
	if (blob && curcol->column_cur_size > curcol->column_size)
		curcol->column_size = curcol->column_cur_size;

	return SQL_SUCCESS;
}
