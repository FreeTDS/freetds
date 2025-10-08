/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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

#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/time.h>
#include <freetds/odbc.h>
#include <freetds/convert.h>
#include <freetds/iconv.h>
#include <freetds/utils/string.h>
#include <freetds/utils.h>

TDS_INT
convert_datetime2server(int bindtype, const void *src, TDS_DATETIMEALL * dta)
{
	struct tm src_tm;
	int tm_dms;
	unsigned int dt_time;
	int i;
	time_t curr_time;

	const DATE_STRUCT *src_date = (const DATE_STRUCT *) src;
	const TIME_STRUCT *src_time = (const TIME_STRUCT *) src;
	const TIMESTAMP_STRUCT *src_timestamp = (const TIMESTAMP_STRUCT *) src;

	memset(dta, 0, sizeof(*dta));

	switch (bindtype) {
	case SQL_C_DATE:
	case SQL_C_TYPE_DATE:
		src_tm.tm_year = src_date->year - 1900;
		src_tm.tm_mon = src_date->month - 1;
		src_tm.tm_mday = src_date->day;
		src_tm.tm_hour = 0;
		src_tm.tm_min = 0;
		src_tm.tm_sec = 0;
		tm_dms = 0;
		dta->has_date = 1;
		break;
	case SQL_C_TIME:
	case SQL_C_TYPE_TIME:
#if HAVE_GETTIMEOFDAY
		{
			struct timeval tv;
		        gettimeofday(&tv, NULL);
		        curr_time = tv.tv_sec;
		}
#else
		curr_time = time(NULL);
#endif
		tds_localtime_r(&curr_time, &src_tm);
		src_tm.tm_hour = src_time->hour;
		src_tm.tm_min = src_time->minute;
		src_tm.tm_sec = src_time->second;
		tm_dms = 0;
		dta->has_time = 1;
		break;
	case SQL_C_TIMESTAMP:
	case SQL_C_TYPE_TIMESTAMP:
		src_tm.tm_year = src_timestamp->year - 1900;
		src_tm.tm_mon = src_timestamp->month - 1;
		src_tm.tm_mday = src_timestamp->day;
		src_tm.tm_hour = src_timestamp->hour;
		src_tm.tm_min = src_timestamp->minute;
		src_tm.tm_sec = src_timestamp->second;
		tm_dms = src_timestamp->fraction / 100lu;
		dta->has_date = 1;
		dta->has_time = 1;
		break;
	default:
		return TDS_CONVERT_FAIL;
	}

	/* TODO code copied from convert.c, function */
	i = (src_tm.tm_mon - 13) / 12;
	dta->has_date = 1;
	dta->date = 1461 * (src_tm.tm_year + 300 + i) / 4 +
		(367 * (src_tm.tm_mon - 1 - 12 * i)) / 12 - (3 * ((src_tm.tm_year + 400 + i) / 100)) / 4 +
		src_tm.tm_mday - 109544;

	dta->has_time = 1;
	dt_time = (src_tm.tm_hour * 60 + src_tm.tm_min) * 60 + src_tm.tm_sec;
	dta->time = dt_time * UINT64_C(10000000) + tm_dms;
	return sizeof(TDS_DATETIMEALL);
}

TDS_INT
convert_numeric2server(struct _sql_errors *errs, const void *src, TDS_NUMERIC *num)
{
	const SQL_NUMERIC_STRUCT *sql_num;
	size_t i;

	sql_num = (const SQL_NUMERIC_STRUCT *) src;
	num->precision = sql_num->precision;
	num->scale = sql_num->scale;
	num->array[0] = sql_num->sign ^ 1;
	/* test precision so client do not crash our library */
	if (num->precision <= 0 || num->precision > 38 || num->scale > num->precision) {
		odbc_convert_err_set(errs, TDS_CONVERT_FAIL);
		return TDS_CONVERT_FAIL;
	}
	i = tds_numeric_bytes_per_prec[num->precision];
	memset(num->array + 1, 0, sizeof(num->array) - 1);
	memcpy(num->array + 1, sql_num->val, i - 1);
	tds_swap_bytes(num->array + 1, i - 1);
	return sizeof(TDS_NUMERIC);
}

static char*
odbc_wstr2str(TDS_STMT * stmt, const char *src, int* len)
{
	int srclen = (*len) / sizeof(SQLWCHAR);
	char *out = tds_new(char, srclen + 1), *p;
	const SQLWCHAR *wp = (const SQLWCHAR *) src;

	if (!out) {
		odbc_errs_add(&stmt->errs, "HY001", NULL);
		return NULL;
	}

        /* convert */
        p = out;
	for (; srclen && *wp < 256; --srclen)
		*p++ = (char) *wp++;

	/* still characters, wrong format */
	if (srclen) {
		free(out);
		/* TODO correct error ?? */
		odbc_errs_add(&stmt->errs, "07006", NULL);
		return NULL;
	}

	*len = (int) (p - out);
	return out;
}

static void
_odbc_blob_free(TDSCOLUMN *col)
{
        if (!col->column_data)
                return;

        TDS_ZERO_FREE(col->column_data);
}

static SQLRETURN
odbc_convert_table(TDS_STMT *stmt, SQLTVP *src, TDS_TVP *dest, SQLLEN num_rows)
{
	SQLLEN i;
	int j;
	TDS_TVP_ROW *row;
	TDS_TVP_ROW **prow;
	TDSPARAMINFO *params, *new_params;
	TDS_DESC *apd = src->apd, *ipd = src->ipd;
	SQLRETURN ret;
	char *type_name, *pch;

	/* probably data at execution attempt, currently not supported for TVP */
	if (num_rows < 0) {
		odbc_errs_add(&stmt->errs, "HYC00", "Data at execution are not supported for TVP columns");
		return SQL_ERROR;
	}

	tds_deinit_tvp(dest);

	type_name = strdup(tds_dstr_cstr(&src->type_name));
	if (!type_name)
		goto Memory_Error;

	/* Tokenize and extract the schema & TVP typename from the TVP's full name */
	pch = strchr(type_name, '.');
	if (pch == NULL) {
		dest->schema = tds_strndup("", 0);
		dest->name = type_name;
	} else {
		*pch = 0;
		dest->schema = type_name;
		dest->name = strdup(++pch);
	}
	if (!dest->schema || !dest->name)
		goto Memory_Error;

	/* Ensure that the TVP typename does not contain any more '.' */
	/* Otherwise, report it as an invalid data type error */
	pch = strchr(dest->name, '.');
	if (pch != NULL) {
		odbc_errs_add(&stmt->errs, "HY004", NULL);
		return SQL_ERROR;
	}

	params = NULL;
	for (j = 0; j < ipd->header.sql_desc_count; j++) {
		if (!(new_params = tds_alloc_param_result(params))) {
			tds_free_param_results(params);
			goto Memory_Error;
		}
		params = new_params;

		ret = odbc_sql2tds(stmt, &ipd->records[j], &apd->records[j], new_params->columns[j], false, apd, 0);
		if (!SQL_SUCCEEDED(ret)) {
			tds_free_param_results(params);
			return ret;
		}
	}
	dest->metadata = params;

	for (i = 0, prow = &dest->row; i < num_rows; prow = &(*prow)->next, i++) {
		if ((row = tds_new0(TDS_TVP_ROW, 1)) == NULL)
			goto Memory_Error;

		*prow = row;

		params = NULL;
		for (j = 0; j < ipd->header.sql_desc_count; j++) {
			if (!(new_params = tds_alloc_param_result(params))) {
				tds_free_param_results(params);
				goto Memory_Error;
			}
			params = new_params;

			ret = odbc_sql2tds(stmt, &ipd->records[j], &apd->records[j], new_params->columns[j], true, apd, i);
			if (!SQL_SUCCEEDED(ret)) {
				tds_free_param_results(params);
				return ret;
			}
		}
		row->params = params;
	}

	return SQL_SUCCESS;

Memory_Error:
	odbc_errs_add(&stmt->errs, "HY001", NULL);
	return SQL_ERROR;
}

/**
 * Convert parameters to libtds format
 * @param stmt        ODBC statement
 * @param drec_ixd    IRD or IPD record of destination
 * @param drec_ard    ARD or APD record of source
 * @param curcol      destination column
 * @param compute_row true if data needs to be written to column
 * @param axd         ARD or APD of source
 * @param n_row       number of the row to write
 * @return SQL_SUCCESS, SQL_ERROR or SQL_NEED_DATA
 */
SQLRETURN
odbc_sql2tds(TDS_STMT * stmt, const struct _drecord *drec_ixd, const struct _drecord *drec_axd, TDSCOLUMN *curcol,
	bool compute_row, const TDS_DESC* axd, SQLSETPOSIROW n_row)
{
	TDS_DBC * dbc = stmt->dbc;
	TDSCONNECTION * conn = dbc->tds_socket->conn;
	TDS_SERVER_TYPE dest_type, src_type;
	int sql_src_type, res;
	CONV_RESULT ores;
	TDSBLOB *blob;
	char *src, *converted_src;
	unsigned char *dest;
	int len;
	ODBC_CONVERT_BUF convert_buf;
	SQLLEN sql_len;
	bool need_data = false;

	/* TODO handle bindings of char like "{d '2002-11-12'}" */
	tdsdump_log(TDS_DBG_INFO2, "type=%d\n", drec_ixd->sql_desc_concise_type);

	/* what type to convert ? */
	dest_type = odbc_sql_to_server_type(conn, drec_ixd->sql_desc_concise_type, drec_ixd->sql_desc_unsigned);
	if (dest_type == TDS_INVALID_TYPE) {
		odbc_errs_add(&stmt->errs, "07006", NULL);	/* Restricted data type attribute violation */
		return SQL_ERROR;
	}
	tdsdump_log(TDS_DBG_INFO2, "trace\n");

	/* get C type */
	sql_src_type = drec_axd->sql_desc_concise_type;
	if (sql_src_type == SQL_C_DEFAULT)
		sql_src_type = odbc_sql_to_c_type_default(drec_ixd->sql_desc_concise_type);

	tds_set_param_type(conn, curcol, dest_type);

	/* TODO what happen for unicode types ?? */
	if (is_char_type(dest_type)) {
		TDSICONV *conv = conn->char_convs[is_unicode_type(dest_type) ? client2ucs2 : client2server_chardata];

		/* use binary format for binary to char */
		if (sql_src_type == SQL_C_BINARY) {
			curcol->char_conv = NULL;
		} else if (sql_src_type == SQL_C_WCHAR) {
			curcol->char_conv = tds_iconv_get_info(conn, odbc_get_wide_canonic(conn), conv->to.charset.canonic);
			memcpy(curcol->column_collation, conn->collation, sizeof(conn->collation));
		} else {
#ifdef ENABLE_ODBC_WIDE
			curcol->char_conv = tds_iconv_get_info(conn, dbc->original_charset_num, conv->to.charset.canonic);
#else
			curcol->char_conv = NULL;
#endif
		}
	}
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = (TDS_TINYINT) drec_ixd->sql_desc_precision;
		curcol->column_scale = (TDS_TINYINT) drec_ixd->sql_desc_scale;
	}

	if (drec_ixd->sql_desc_parameter_type != SQL_PARAM_INPUT)
		curcol->column_output = 1;

	/* compute destination length */
	if (curcol->column_varint_size != 0) {
		/* curcol->column_size = drec_axd->sql_desc_octet_length; */
		/*
		 * TODO destination length should come from sql_desc_length, 
		 * however there is the encoding problem to take into account
		 * we should fill destination length after conversion keeping 
		 * attention to fill correctly blob/fixed type/variable type
		 */
		/* TODO location of this test is correct here ?? */
		if (dest_type != SYBUNIQUE && !is_fixed_type(dest_type)) {
			curcol->column_cur_size = 0;
			curcol->column_size = drec_ixd->sql_desc_length;
			/* Ensure that the column_cur_size and column_size are consistent, */
			/* since drec_ixd->sql_desc_length contains the number of rows for a TVP */
			if (dest_type == SYBMSTABLE)
				curcol->column_size = sizeof(TDS_TVP);
			if (curcol->column_size < 0) {
				curcol->on_server.column_size = curcol->column_size = 0x7FFFFFFFl;
			} else {
				if (is_unicode_type(dest_type))
					curcol->on_server.column_size = curcol->column_size * 2;
				else
					curcol->on_server.column_size = curcol->column_size;
			}
		}
	} else if (dest_type != SYBBIT) {
		/* TODO only a trick... */
		tds_set_param_type(conn, curcol, tds_get_null_type(dest_type));
	}

	/* test source type */
	/* TODO test intervals */
	src_type = odbc_c_to_server_type(sql_src_type);
	if (!src_type) {
		odbc_errs_add(&stmt->errs, "07006", NULL);	/* Restricted data type attribute violation */
		return SQL_ERROR;
	}

	/* we have no data to convert, just return */
	if (!compute_row)
		return SQL_SUCCESS;

	src = (char *) drec_axd->sql_desc_data_ptr;
	if (src && n_row) {
		SQLLEN len;
		if (axd->header.sql_desc_bind_type != SQL_BIND_BY_COLUMN) {
			len = axd->header.sql_desc_bind_type;
			if (axd->header.sql_desc_bind_offset_ptr)
				src += *axd->header.sql_desc_bind_offset_ptr;
		} else {
			len = odbc_get_octet_len(sql_src_type, drec_axd);
			if (len < 0)
				/* TODO sure ? what happen to upper layer ?? */
				/* TODO fill error */
				return SQL_ERROR;
		}
		src += len * n_row;
	}

	/* if only output assume input is NULL */
	if (drec_ixd->sql_desc_parameter_type == SQL_PARAM_OUTPUT) {
		sql_len = SQL_NULL_DATA;
	} else {
		sql_len = odbc_get_param_len(drec_axd, drec_ixd, axd, n_row);

		/* special case, MS ODBC handle conversion from "\0" to any to NULL, DBD::ODBC require it */
		if (src_type == SYBVARCHAR && sql_len == 1 && drec_ixd->sql_desc_parameter_type == SQL_PARAM_INPUT_OUTPUT
		    && src && *src == 0) {
			sql_len = SQL_NULL_DATA;
		}
	}

	/* compute source length */
	switch (sql_len) {
	case SQL_NULL_DATA:
		len = 0;
		break;
	case SQL_NTS:
		/* check that SQLBindParameter::ParameterValuePtr is not zero for input parameters */
		if (!src) {
			odbc_errs_add(&stmt->errs, "HY090", NULL);
			return SQL_ERROR;
		}
		if (sql_src_type == SQL_C_WCHAR)
			len = (int) sqlwcslen((const SQLWCHAR *) src) * sizeof(SQLWCHAR);
		else
			len = (int) strlen(src);
		break;
	case SQL_DEFAULT_PARAM:
		odbc_errs_add(&stmt->errs, "07S01", NULL);	/* Invalid use of default parameter */
		return SQL_ERROR;
		break;
	case SQL_DATA_AT_EXEC:
	default:
		len = sql_len;
		if (sql_len < 0) {
			/* test for SQL_C_CHAR/SQL_C_BINARY */
			switch (sql_src_type) {
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_BINARY:
				break;
			default:
				odbc_errs_add(&stmt->errs, "HY090", NULL);
				return SQL_ERROR;
			}
			len = SQL_LEN_DATA_AT_EXEC(sql_len);
			need_data = true;

			/* dynamic length allowed only for variable fields */
			switch (drec_ixd->sql_desc_concise_type) {
			case SQL_CHAR:
			case SQL_VARCHAR:
			case SQL_LONGVARCHAR:
			case SQL_WCHAR:
			case SQL_WVARCHAR:
			case SQL_WLONGVARCHAR:
			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
				break;
			default:
				odbc_errs_add(&stmt->errs, "HY090", NULL);
				return SQL_ERROR;
			}
		}
	}

	/* set NULL. For NULLs we don't need to allocate row buffer so avoid it */
	if (!need_data) {
		assert(drec_ixd->sql_desc_parameter_type != SQL_PARAM_OUTPUT || sql_len == SQL_NULL_DATA);
		if (sql_len == SQL_NULL_DATA) {
			curcol->column_cur_size = -1;
			return SQL_SUCCESS;
		}
	}

	if (is_char_type(dest_type) && !need_data
	    && (sql_src_type == SQL_C_CHAR || sql_src_type == SQL_C_WCHAR || sql_src_type == SQL_C_BINARY)) {
		if (curcol->column_data && curcol->column_data_free)
			curcol->column_data_free(curcol);
		curcol->column_data_free = NULL;
		if (is_blob_col(curcol)) {
			/* trick to set blob without freeing it, _odbc_blob_free does not free TDSBLOB->textvalue */
			TDSBLOB *blob = tds_new0(TDSBLOB, 1);
			if (!blob) {
				odbc_errs_add(&stmt->errs, "HY001", NULL);
				return SQL_ERROR;
			}
			blob->textvalue = src;
			curcol->column_data = (TDS_UCHAR*) blob;
			curcol->column_data_free = _odbc_blob_free;
		} else {
			curcol->column_data = (TDS_UCHAR*) src;
		}
		curcol->column_size = len;
		curcol->column_cur_size = len;
		return SQL_SUCCESS;
	}

	/* allocate given space */
	if (!tds_alloc_param_data(curcol)) {
		odbc_errs_add(&stmt->errs, "HY001", NULL);
		return SQL_ERROR;
	}

	/* fill data with SQLPutData */
	if (need_data) {
		curcol->column_cur_size = 0;
		return SQL_NEED_DATA;
	}

	if (!src) {
		odbc_errs_add(&stmt->errs, "HY090", NULL);
		return SQL_ERROR;
	}

	/* convert special parameters (not libTDS compatible) */
	switch (src_type) {
	case SYBMSDATETIME2:
		convert_datetime2server(sql_src_type, src, &convert_buf.dta);
		src = (char *) &convert_buf.dta;
		break;
	case SYBDECIMAL:
	case SYBNUMERIC:
		if (convert_numeric2server(&stmt->errs, src, &convert_buf.num) <= 0)
			return SQL_ERROR;
		src = (char *) &convert_buf.num;
		break;
		/* TODO intervals */
	default:
		/* prevents some compiler warnings about missing cases */
		break;
	}

	converted_src = NULL;
	if (sql_src_type == SQL_C_WCHAR) {
		converted_src = src = odbc_wstr2str(stmt, src, &len);
		if (!src)
			/* TODO add proper error */
			return SQL_ERROR;
		src_type = SYBVARCHAR;
	}

	dest = curcol->column_data;
	switch ((TDS_SERVER_TYPE) dest_type) {
	case SYBCHAR:
	case SYBVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case SYBNVARCHAR:
		ores.cc.c = (TDS_CHAR*) dest;
		ores.cc.len = curcol->column_size;
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, TDS_CONVERT_CHAR, &ores);
		if (res > curcol->column_size)
			res = curcol->column_size;
		break;
	case SYBBINARY:
	case SYBVARBINARY:
	case XSYBBINARY:
	case XSYBVARBINARY:
		ores.cb.ib = (TDS_CHAR*) dest;
		ores.cb.len = curcol->column_size;
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, TDS_CONVERT_BINARY, &ores);
		if (res > curcol->column_size)
			res = curcol->column_size;
		break;
	case SYBNTEXT:
		dest_type = SYBTEXT;
	case SYBTEXT:
	case SYBLONGBINARY:
	case SYBIMAGE:
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, dest_type, &ores);
		if (res >= 0) {
			blob = (TDSBLOB *) dest;
			free(blob->textvalue);
			blob->textvalue = ores.ib;
		}
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		((TDS_NUMERIC *) dest)->precision = (unsigned char) drec_ixd->sql_desc_precision;
		((TDS_NUMERIC *) dest)->scale = (unsigned char) drec_ixd->sql_desc_scale;
	case SYBINTN:
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBFLT8:
	case SYBDATETIME:
	case SYBBIT:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBREAL:
	case SYBBITN:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
	case SYBUNIQUE:
	case SYBMSTIME:
	case SYBMSDATE:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, dest_type, (CONV_RESULT*) dest);
		break;
	case SYBMSTABLE:
		free(converted_src);
		src = drec_ixd->sql_desc_data_ptr;
		curcol->column_cur_size = sizeof(TDS_TVP);
		return odbc_convert_table(stmt, (SQLTVP *) src, (TDS_TVP *) dest,
					  drec_axd->sql_desc_octet_length_ptr == NULL ? 1 : *drec_axd->sql_desc_octet_length_ptr);
	default:
	case SYBVOID:
	case SYBVARIANT:
		/* TODO ODBC 3.5 */
		assert(0);
		res = -1;
		break;
	}

	free(converted_src);
	if (res < 0) {
		odbc_convert_err_set(&stmt->errs, res);
		return SQL_ERROR;
	}

	curcol->column_cur_size = res;

	return SQL_SUCCESS;
}
