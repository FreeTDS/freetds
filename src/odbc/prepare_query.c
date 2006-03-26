/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
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

#include "tdsodbc.h"
#include "tdsconvert.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: prepare_query.c,v 1.60 2006-03-26 17:01:36 freddy77 Exp $");

#define TDS_ISSPACE(c) isspace((unsigned char) (c))

static int
prepared_rpc(struct _hstmt *stmt, int compute_row)
{
	int nparam = stmt->params ? stmt->params->num_cols : 0;
	const char *p = stmt->prepared_pos - 1;

	for (;;) {
		TDSPARAMINFO *temp_params;
		TDSCOLUMN *curcol;
		TDS_SERVER_TYPE type;
		const char *start;

		while (TDS_ISSPACE(*++p));
		if (!*p)
			return SQL_SUCCESS;

		/* we have certainly a parameter */
		if (!(temp_params = tds_alloc_param_result(stmt->params))) {
			odbc_errs_add(&stmt->errs, "HY001", NULL);
			return SQL_ERROR;
		}
		stmt->params = temp_params;
		curcol = temp_params->columns[nparam];

		switch (*p) {
		case ',':
			if (IS_TDS7_PLUS(stmt->dbc->tds_socket)) {
				tds_set_param_type(stmt->dbc->tds_socket, curcol, SYBVOID);
				curcol->column_size = curcol->column_cur_size = 0;
			} else {
				/* TODO is there a better type ? */
				tds_set_param_type(stmt->dbc->tds_socket, curcol, SYBINTN);
				curcol->column_size = curcol->on_server.column_size = 4;
				curcol->column_cur_size = -1;
			}
			if (compute_row)
				if (!tds_alloc_param_data(temp_params, curcol))
					return SQL_ERROR;
			--p;
			break;
		default:
			/* add next parameter to list */
			start = p;

			if (!(p = parse_const_param(p, &type)))
				return SQL_ERROR;
			tds_set_param_type(stmt->dbc->tds_socket, curcol, type);
			switch (type) {
			case SYBVARCHAR:
				curcol->column_size = p - start;
				break;
			case SYBVARBINARY:
				curcol->column_size = (p - start) / 2 -1;
				break;
			default:
				assert(0);
			case SYBINT4:
			case SYBFLT8:
				curcol->column_cur_size = curcol->column_size;
				break;
			}
			/* TODO support other type other than VARCHAR, do not strip escape in prepare_call */
			if (compute_row) {
				char *dest;
				int len;
				CONV_RESULT cr;

				if (!tds_alloc_param_data(temp_params, curcol))
					return SQL_ERROR;
				dest = (char *) curcol->column_data;
				switch (type) {
				case SYBVARCHAR:
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
							dest - (char *) curcol->column_data;
					}
					break;
				case SYBVARBINARY:
					len = tds_convert(NULL, SYBVARCHAR, start, p - start, SYBVARBINARY, &cr);
					if (len >= 0) {
						curcol->column_cur_size = len;
						memcpy(dest, cr.ib, len);
						free(cr.ib);
					}
					break;
				case SYBINT4:
					*((TDS_INT *) dest) = strtol(start, NULL, 10);
					break;
				case SYBFLT8:
					*((TDS_FLOAT *) dest) = strtod(start, NULL);
					break;
				default:
					break;
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
				(stmt, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
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
			odbc_errs_add(&stmt->errs, "HY001", NULL);
			return SQL_ERROR;
		}
		stmt->params = temp_params;

		switch (sql2tds
			(stmt, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
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
		blob = (TDSBLOB *) curcol->column_data;
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
		memcpy(curcol->column_data + curcol->column_cur_size, DataPtr, len);
	}
	curcol->column_cur_size += len;
	if (blob && curcol->column_cur_size > curcol->column_size)
		curcol->column_size = curcol->column_cur_size;

	return SQL_SUCCESS;
}
