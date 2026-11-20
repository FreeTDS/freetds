/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005-2008  Frediano Ziglio
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

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include <freetds/odbc.h>
#include <freetds/tds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/tds/checks.h>

#define TDS_ISSPACE(c) isspace((unsigned char) (c))

static int
prepared_rpc(struct _hstmt *stmt, bool compute_row)
{
	int nparam = stmt->params ? stmt->params->num_cols : 0;
	const char *p;
	TDSCONNECTION *conn = stmt->dbc->tds_socket->conn;

	if (stmt->prepared_pos > tds_dstr_len(&stmt->query))
		return SQL_ERROR;

	p = tds_dstr_cstr(&stmt->query) + stmt->prepared_pos - 1;

	for (;;) {
		TDSPARAMINFO *temp_params;
		TDSCOLUMN *curcol;
		TDS_SERVER_TYPE type;
		const char *start;

		while (TDS_ISSPACE(*++p))
			continue;
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
			if (IS_TDS7_PLUS(conn)) {
				tds_set_param_type(conn, curcol, SYBVOID, 0);
				curcol->column_size = curcol->column_cur_size = 0;
			} else {
				/* TODO is there a better type ? */
				tds_set_param_type(conn, curcol, SYBINTN, 4);
				curcol->column_cur_size = -1;
			}
			if (compute_row)
				if (!tds_alloc_param_data(curcol)) {
					tds_free_param_result(temp_params);
					return SQL_ERROR;
				}
			--p;
			break;
		default:
			/* add next parameter to list */
			start = p;

			if (!(p = parse_const_param(p, &type))) {
				tds_free_param_result(temp_params);
				return SQL_ERROR;
			}
			tds_set_param_type(conn, curcol, type, 0);
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
			case SYBINT8:
			case SYBFLT8:
				curcol->column_cur_size = curcol->column_size;
				break;
			}
			curcol->on_server.column_size = curcol->column_size;
			/* TODO support other type other than VARCHAR, do not strip escape in prepare_call */
			if (compute_row) {
				char *dest;
				int len;
				CONV_RESULT cr;

				if (!tds_alloc_param_data(curcol)) {
					tds_free_param_result(temp_params);
					return SQL_ERROR;
				}
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
					cr.cb.len = curcol->column_size;
					cr.cb.ib = dest;
					len = tds_convert(NULL, SYBVARCHAR, start, p - start, TDS_CONVERT_BINARY, &cr);
					if (len >= 0 && len <= curcol->column_size)
						curcol->column_cur_size = len;
					break;
				case SYBINT4:
					*((TDS_INT *) dest) = atoi(start);
					break;
				case SYBINT8:
					*((TDS_INT8 *) dest) = tds_strtoll(start, NULL, 10);
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
			/* find bound parameter */
			if (stmt->param_num > stmt->apd->header.sql_desc_count
			    || stmt->param_num > stmt->ipd->header.sql_desc_count) {
				tds_free_param_result(temp_params);
				/* TODO set error */
				return SQL_ERROR;
			}

			switch (odbc_sql2tds
				(stmt, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
				 curcol, compute_row, stmt->apd, stmt->curr_param_row)) {
			case SQL_ERROR:
				return SQL_ERROR;
			case SQL_NEED_DATA:
				return SQL_NEED_DATA;
			}
			++stmt->param_num;
			break;
		}
		++nparam;

		while (TDS_ISSPACE(*++p))
			continue;
		if (!*p || *p != ',')
			return SQL_SUCCESS;
		stmt->prepared_pos = p + 1 - tds_dstr_cstr(&stmt->query);
	}
}

void
odbc_reset_dae_conversion(struct _hstmt *stmt)
{
	if (!stmt)
		return;

	tdsdump_log(TDS_DBG_FUNC, "resetting DAE conversion\n");

	free(stmt->dae_param_conversion.data);
	memset(&stmt->dae_param_conversion, 0, sizeof(ODBC_DAE_PARAM_CONV));
}

int
odbc_append_dae_data(struct _hstmt *stmt, const void *data, SQLLEN length)
{
    ODBC_DAE_PARAM_CONV *dae;
    TDS_UCHAR *buffer;
    size_t chunk_length;
    size_t required;
    size_t capacity;

    if (!stmt || length < 0 || (!data && length > 0)) {
        if (stmt)
            odbc_errs_add(&stmt->errs, "HY090", NULL);
        return SQL_ERROR;
    }

    tdsdump_log(TDS_DBG_FUNC, "Appending DAE data\n");

    dae = &stmt->dae_param_conversion;
    if (dae->mode != ODBC_DAE_BUFFER_CONVERT
            || stmt->param_num <= 0
            || dae->parameter_index != (size_t) stmt->param_num - 1
            || dae->row_index != (SQLULEN) stmt->curr_param_row) {
        odbc_errs_add(&stmt->errs, "HY010", NULL);
        odbc_reset_dae_conversion(stmt);
        return SQL_ERROR;
    }

    chunk_length = (size_t) length;

    if (chunk_length > SIZE_MAX - dae->length) {
        odbc_errs_add(&stmt->errs, "22001", NULL);
        odbc_reset_dae_conversion(stmt);
        return SQL_ERROR;
    }
    required = dae->length + chunk_length;

    if (required > dae->capacity) {
        capacity = dae->capacity ? dae->capacity : 32;

        while (capacity < required) {
            if (capacity > SIZE_MAX / 2) {
                capacity = required;
                break;
            }
            capacity *= 2;
        }

        buffer = (TDS_UCHAR *) realloc(dae->data, capacity);
        if (!buffer) {
            odbc_errs_add(&stmt->errs, "HY001", NULL);
            odbc_reset_dae_conversion(stmt);
            return SQL_ERROR;
        }

        dae->data = buffer;
        dae->capacity = capacity;
    }

    if (chunk_length > 0) {
        memcpy(dae->data + dae->length, data, chunk_length);
    }

    dae->length = required;
    return SQL_SUCCESS;
}

int
odbc_finish_dae_conversion(struct _hstmt *stmt)
{
    ODBC_DAE_PARAM_CONV *dae;
    const struct _drecord *drec_ipd;
    TDSCOLUMN *curcol;
    size_t column_index;
    size_t parameter_offset;
    int result;

    if (!stmt)
        return SQL_ERROR;

    dae = &stmt->dae_param_conversion;
    if (dae->mode == ODBC_DAE_INVALID)
        return SQL_SUCCESS;
    if (dae->mode != ODBC_DAE_BUFFER_CONVERT) {
        odbc_reset_dae_conversion(stmt);
        return SQL_SUCCESS;
    }

    tdsdump_log(TDS_DBG_FUNC, "Finalizing conversion of buffered DAE data\n");

    if (       stmt->param_num <= 0
            || dae->parameter_index != (size_t) stmt->param_num - 1
            || dae->row_index != (SQLULEN) stmt->curr_param_row
	) {
        odbc_errs_add(&stmt->errs, "HY010", NULL);
        odbc_reset_dae_conversion(stmt);
        return SQL_ERROR;
    }
    parameter_offset = stmt->prepared_query_is_func ? 2u : 1u;

    if ((size_t) stmt->param_num < parameter_offset) {
        odbc_errs_add(&stmt->errs, "HY010", NULL);
        odbc_reset_dae_conversion(stmt);
        return SQL_ERROR;
    }
    column_index = (size_t) stmt->param_num - parameter_offset;

    if (!stmt->params
        || !stmt->ipd
        || column_index >= (size_t) stmt->params->num_cols
        || dae->parameter_index >= (size_t) stmt->ipd->header.sql_desc_count
    ) {
        odbc_errs_add(&stmt->errs, "HY000", NULL);
        odbc_reset_dae_conversion(stmt);
        return SQL_ERROR;
    }
    drec_ipd = &stmt->ipd->records[dae->parameter_index];
    curcol = stmt->params->columns[column_index];

    result = odbc_convert_param_data(stmt, drec_ipd, curcol, dae->source_c_type, dae->source_tds_type, dae->data, dae->length, dae->destination_tds_type);

    odbc_reset_dae_conversion(stmt);
    return result;
}

int
parse_prepared_query(struct _hstmt *stmt, bool compute_row)
{
	/* try setting this parameter */
	TDSPARAMINFO *temp_params;
	int nparam = stmt->params ? stmt->params->num_cols : 0;

	if (stmt->prepared_pos > 0)
		return prepared_rpc(stmt, compute_row);

	tdsdump_log(TDS_DBG_FUNC, "parsing %d parameters\n", nparam);

	for (; stmt->param_num <= (int) stmt->param_count; ++nparam, ++stmt->param_num) {
		/* find bound parameter */
		if (stmt->param_num > stmt->apd->header.sql_desc_count || stmt->param_num > stmt->ipd->header.sql_desc_count) {
			tdsdump_log(TDS_DBG_FUNC, "parse_prepared_query: logic_error: parameter out of bounds: "
						  "%d > %d || %d > %d\n",
						   stmt->param_num, stmt->apd->header.sql_desc_count,
						   stmt->param_num, stmt->ipd->header.sql_desc_count);
			return SQL_ERROR;
		}

		/* add a column to parameters */
		if (!(temp_params = tds_alloc_param_result(stmt->params))) {
			odbc_errs_add(&stmt->errs, "HY001", NULL);
			return SQL_ERROR;
		}
		stmt->params = temp_params;

		switch (odbc_sql2tds
			(stmt, &stmt->ipd->records[stmt->param_num - 1], &stmt->apd->records[stmt->param_num - 1],
			 stmt->params->columns[nparam], compute_row, stmt->apd, stmt->curr_param_row)) {
		case SQL_ERROR:
			return SQL_ERROR;
		case SQL_NEED_DATA:
			return SQL_NEED_DATA;
		}
	}
	return SQL_SUCCESS;
}

int
start_parse_prepared_query(struct _hstmt *stmt, bool compute_row)
{
	/* TODO should be NULL already ?? */
	tds_free_param_results(stmt->params);
	stmt->params = NULL;
	odbc_reset_dae_conversion(stmt);

	stmt->param_num = stmt->prepared_query_is_func ? 2 : 1;
	return parse_prepared_query(stmt, compute_row);
}

static ptrdiff_t
odbc_wchar2hex(TDS_CHAR *dest, size_t destlen, const SQLWCHAR * src, size_t srclen)
{
	size_t i;
	SQLWCHAR hex1, c = 0;

	/* if srclen if odd we must add a "0" before ... */
	i = 0;		/* number where to start converting */
	if (srclen & 1) {
		++srclen;
		i = 1;
		--src;
	}
	for (; i < srclen; ++i) {
		hex1 = src[i];

		if ('0' <= hex1 && hex1 <= '9')
			hex1 &= 0x0f;
		else {
			hex1 |= 0x20;	/* ensure lower case */
			if ('a' <= hex1 && hex1 <= 'f') {
				hex1 -= ('a' - 10);
			} else {
				tdsdump_log(TDS_DBG_INFO1,
					    "error_handler:  attempt to convert data stopped by syntax error in source field \n");
				return TDS_CONVERT_SYNTAX;
			}
		}
		tds_extra_assert(hex1 < 0x10);

		if ((i/2u) >= destlen)
			continue;

		if (i & 1)
			dest[i / 2u] = c | hex1;
		else
			c = hex1 << 4;
	}
	return srclen / 2u;
}

int
continue_parse_prepared_query(struct _hstmt *stmt, SQLPOINTER DataPtr, SQLLEN StrLen_or_Ind)
{
	struct _drecord *drec_apd, *drec_ipd;
	SQLLEN len;
	int need_bytes;
	TDSCOLUMN *curcol;
	TDSBLOB *blob;
	int sql_src_type;

	assert(stmt);

	tdsdump_log(TDS_DBG_FUNC, "continue_parse_prepared_query with parameter %d\n", stmt->param_num);

	if (!stmt->params) {
		tdsdump_log(TDS_DBG_FUNC, "error? continue_parse_prepared_query: no parameters provided");
		return SQL_ERROR;
	}

	if (stmt->param_num > stmt->apd->header.sql_desc_count || stmt->param_num > stmt->ipd->header.sql_desc_count)
		return SQL_ERROR;
	drec_apd = &stmt->apd->records[stmt->param_num - 1];
	drec_ipd = &stmt->ipd->records[stmt->param_num - 1];

	curcol = stmt->params->columns[stmt->param_num - (stmt->prepared_query_is_func ? 2 : 1)];
	blob = NULL;
	if (is_blob_col(curcol))
		blob = (TDSBLOB *) curcol->column_data;
	assert(curcol->column_cur_size <= curcol->column_size);
	need_bytes = curcol->column_size - curcol->column_cur_size;

	if (DataPtr == NULL) {
		switch(StrLen_or_Ind) {
		case SQL_NULL_DATA:
		case SQL_DEFAULT_PARAM:
			break;	/* OK */
		default:
			odbc_errs_add(&stmt->errs, "HY009", NULL); /* Invalid use of null pointer */
			return SQL_ERROR;
		}
	}

	/* get C type */
	sql_src_type = drec_apd->sql_desc_concise_type;
	if (sql_src_type == SQL_C_DEFAULT)
		sql_src_type = odbc_sql_to_c_type_default(drec_ipd->sql_desc_concise_type);

	switch(StrLen_or_Ind) {
	case SQL_NTS:
		if (sql_src_type == SQL_C_WCHAR)
			len = sqlwcslen((SQLWCHAR *) DataPtr);
		else
			len = strlen((char *) DataPtr);
		break;
	case SQL_NULL_DATA:
		len = 0;
		break;
	case SQL_DEFAULT_PARAM:
		/* FIXME: use the default if the parameter has one. */
		odbc_errs_add(&stmt->errs, "07S01", NULL); /* Invalid use of default parameter */
		return SQL_ERROR;
	default:
		if (DataPtr && StrLen_or_Ind < 0) {
			/*
			 * "The argument DataPtr was not a null pointer, and
			 * the argument StrLen_or_Ind was less than 0
			 * but not equal to SQL_NTS or SQL_NULL_DATA."
			 */
			odbc_errs_add(&stmt->errs, "HY090", NULL);
			return SQL_ERROR;
		}
		len = StrLen_or_Ind;
		break;
	}

	if (StrLen_or_Ind == SQL_NULL_DATA) {
		curcol->column_cur_size = -1;
		odbc_reset_dae_conversion(stmt);
		return SQL_SUCCESS;
	}

	if (stmt->dae_param_conversion.mode == ODBC_DAE_BUFFER_CONVERT)
		return odbc_append_dae_data(stmt, DataPtr, len);

	if (!blob && len > need_bytes) {
		odbc_errs_add(&stmt->errs, "22001", NULL);
		return SQL_ERROR;
	}

	/* copy to destination */
	if (blob) {
		TDS_CHAR *p;
		int binary_convert = 0;
		SQLLEN orig_len = len;

		if (sql_src_type == SQL_C_CHAR || sql_src_type == SQL_C_WCHAR) {
			TDS_SERVER_TYPE type = tds_get_conversion_type(curcol->column_type, curcol->column_size);
			if (is_binary_type(type)) {
				if (len && sql_src_type == SQL_C_CHAR && !*((char*)DataPtr+len-1))
					--len;

				if (sql_src_type == SQL_C_WCHAR)
					len /= sizeof(SQLWCHAR);

				if (!len)
					return SQL_SUCCESS;

				binary_convert = 1;
				orig_len = len;
				len = len / 2u + 1u;
			}
		}

		if (!len)
			return SQL_SUCCESS;

		assert(blob->textvalue || curcol->column_cur_size == 0);
		p = (TDS_CHAR *) TDS_RESIZE(blob->textvalue, len + curcol->column_cur_size);
		if (!p) {
			odbc_errs_add(&stmt->errs, "HY001", NULL); /* Memory allocation error */
			return SQL_ERROR;
		}

		p += curcol->column_cur_size;
		if (binary_convert) {
			ptrdiff_t res;

			len = orig_len;

			if (curcol->column_cur_size > 0
			&&  curcol->column_text_sqlputdatainfo) {
				SQLWCHAR data[2];
				data[0] = curcol->column_text_sqlputdatainfo;
				data[1] = (sql_src_type == SQL_C_CHAR) ? *(unsigned char*)DataPtr : *(SQLWCHAR*)DataPtr;

				res = odbc_wchar2hex(p, 1, data, 2);
				if (res < 0) {
					odbc_convert_err_set(&stmt->errs, (TDS_INT) res);
					return SQL_ERROR;
				}
				p += res;

				DataPtr = (SQLPOINTER) (((char*)DataPtr) +
					(sql_src_type == SQL_C_CHAR ? 1 : sizeof(SQLWCHAR)));
				--len;
			}

			if (len&1) {
				--len;
				curcol->column_text_sqlputdatainfo =
					(sql_src_type == SQL_C_CHAR) ? ((char*)DataPtr)[len] : ((SQLWCHAR*)DataPtr)[len];
			}

			res = (sql_src_type == SQL_C_CHAR) ?
				tds_char2hex(p, len / 2u, (const TDS_CHAR*) DataPtr, len):
				odbc_wchar2hex(p, len / 2u, (const SQLWCHAR*) DataPtr, len);
			if (res < 0) {
				odbc_convert_err_set(&stmt->errs, (TDS_INT) res);
				return SQL_ERROR;
			}
			p += res;

			len = p - (blob->textvalue + curcol->column_cur_size);
		} else {
			memcpy(blob->textvalue + curcol->column_cur_size, DataPtr, len);
		}
	} else if (len > 0) {
		memcpy(curcol->column_data + curcol->column_cur_size, DataPtr, len);
	}

	curcol->column_cur_size += len;

	if (blob && curcol->column_cur_size > curcol->column_size)
		curcol->column_size = curcol->column_cur_size;

	return SQL_SUCCESS;
}
