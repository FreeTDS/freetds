/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005 Frediano Ziglio
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
#endif /* HAVE_CONFIG_H */

#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "tdsodbc.h"
#include "tdsconvert.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: sql2tds.c,v 1.57 2006-06-30 14:34:40 jklowden Exp $");

static TDS_INT
convert_datetime2server(int bindtype, const void *src, TDS_DATETIME * dt)
{
	struct tds_time src_tm;
	unsigned int dt_time;
	TDS_INT dt_days;
	int i;
	int got_date = 1;
	time_t curr_time;

	const DATE_STRUCT *src_date = (const DATE_STRUCT *) src;
	const TIME_STRUCT *src_time = (const TIME_STRUCT *) src;
	const TIMESTAMP_STRUCT *src_timestamp = (const TIMESTAMP_STRUCT *) src;

	switch (bindtype) {
	case SQL_C_DATE:
	case SQL_C_TYPE_DATE:
		src_tm.tm_year = src_date->year - 1900;
		src_tm.tm_mon = src_date->month - 1;
		src_tm.tm_mday = src_date->day;
		src_tm.tm_hour = 0;
		src_tm.tm_min = 0;
		src_tm.tm_sec = 0;
		src_tm.tm_ms = 0;
		break;
	case SQL_C_TIME:
	case SQL_C_TYPE_TIME:
		got_date = 0;
#if HAVE_GETTIMEOFDAY
		{
			struct timeval tv;
		        gettimeofday(&tv, NULL);
		        curr_time = tv.tv_sec;
		}
#else
		curr_time = time(NULL);
#endif
		dt_days = (curr_time / 86400u) + (70u * 365u + 17u);
		src_tm.tm_hour = src_time->hour;
		src_tm.tm_min = src_time->minute;
		src_tm.tm_sec = src_time->second;
		src_tm.tm_ms = 0;
		break;
	case SQL_C_TIMESTAMP:
	case SQL_C_TYPE_TIMESTAMP:
		src_tm.tm_year = src_timestamp->year - 1900;
		src_tm.tm_mon = src_timestamp->month - 1;
		src_tm.tm_mday = src_timestamp->day;
		src_tm.tm_hour = src_timestamp->hour;
		src_tm.tm_min = src_timestamp->minute;
		src_tm.tm_sec = src_timestamp->second;
		src_tm.tm_ms = src_timestamp->fraction / 1000000lu;
		break;
	default:
		return TDS_FAIL;
	}

	/* TODO code copied from convert.c, function */
	if (got_date) {
		i = (src_tm.tm_mon - 13) / 12;
		dt_days = 1461 * (src_tm.tm_year + 300 + i) / 4 +
			(367 * (src_tm.tm_mon - 1 - 12 * i)) / 12 - (3 * ((src_tm.tm_year + 400 + i) / 100)) / 4 +
			src_tm.tm_mday - 109544;
	}

	dt->dtdays = dt_days;
	dt_time = (src_tm.tm_hour * 60 + src_tm.tm_min) * 60 + src_tm.tm_sec;
	dt->dttime = dt_time * 300 + (src_tm.tm_ms * 300 / 1000);
	return sizeof(TDS_DATETIME);
}

/**
 * Convert parameters to libtds format
 * @return SQL_SUCCESS, SQL_ERROR or SQL_NEED_DATA
 */
SQLRETURN
sql2tds(TDS_STMT * stmt, const struct _drecord *drec_ipd, const struct _drecord *drec_apd, TDSPARAMINFO * info, int nparam,
	int compute_row)
{
	TDS_DBC * dbc = stmt->dbc;
	int dest_type, src_type, sql_src_type, res;
	CONV_RESULT ores;
	TDSBLOB *blob;
	char *src;
	unsigned char *dest;
	TDSCOLUMN *curcol = info->columns[nparam];
	int len;
	TDS_DATETIME dt;
	TDS_NUMERIC num;
	SQL_NUMERIC_STRUCT *sql_num;
	SQLINTEGER sql_len;
	int need_data = 0, i;

	/* TODO handle bindings of char like "{d '2002-11-12'}" */
	tdsdump_log(TDS_DBG_INFO2, "type=%d\n", drec_ipd->sql_desc_concise_type);

	/* what type to convert ? */
	dest_type = odbc_sql_to_server_type(dbc->tds_socket, drec_ipd->sql_desc_concise_type);
	if (dest_type == TDS_FAIL)
		return SQL_ERROR;
	tdsdump_log(TDS_DBG_INFO2, "trace\n");

	/* TODO what happen for unicode types ?? */
	tds_set_param_type(dbc->tds_socket, curcol, dest_type);
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = drec_ipd->sql_desc_precision;
		curcol->column_scale = drec_ipd->sql_desc_scale;
	}

	if (drec_ipd->sql_desc_parameter_type != SQL_PARAM_INPUT)
		curcol->column_output = 1;

	/* compute destination length */
	if (curcol->column_varint_size != 0) {
		/* curcol->column_size = drec_apd->sql_desc_octet_length; */
		/*
		 * TODO destination length should come from sql_desc_length, 
		 * however there is the encoding problem to take into account
		 * we should fill destination length after conversion keeping 
		 * attention to fill correctly blob/fixed type/variable type
		 */
		/* TODO location of this test is correct here ?? */
		if (dest_type != SYBUNIQUE && dest_type != SYBBITN && !is_fixed_type(dest_type)) {
			curcol->column_cur_size = 0;
			curcol->column_size = drec_ipd->sql_desc_length;
			if (curcol->column_size < 0)
				curcol->column_size = 0x7FFFFFFFl;
		}
	} else if (dest_type != SYBBIT) {
		/* TODO only a trick... */
		tds_set_param_type(dbc->tds_socket, curcol, tds_get_null_type(dest_type));
	}

	/* get C type */
	sql_src_type = drec_apd->sql_desc_concise_type;
	if (sql_src_type == SQL_C_DEFAULT)
		sql_src_type = odbc_sql_to_c_type_default(drec_ipd->sql_desc_concise_type);

	/* test source type */
	/* TODO test intervals */
	src_type = odbc_c_to_server_type(sql_src_type);
	if (src_type == TDS_FAIL)
		return SQL_ERROR;

	/* we have no data to convert, just return */
	if (!compute_row)
		return TDS_SUCCEED;

	src = drec_apd->sql_desc_data_ptr;
	if (src && stmt->curr_param_row) {
		if (stmt->apd->header.sql_desc_bind_type != SQL_BIND_BY_COLUMN) {
			src += stmt->apd->header.sql_desc_bind_type;
			if (stmt->apd->header.sql_desc_bind_offset_ptr)
				src += *stmt->apd->header.sql_desc_bind_offset_ptr;
		} else {
			SQLLEN len;

			/* this shit is mine -- freddy77 */
			switch (sql_src_type) {
			case SQL_C_CHAR:
			case SQL_C_BINARY:
				len = drec_apd->sql_desc_octet_length;
				break;
			case SQL_C_DATE:
			case SQL_C_TYPE_DATE:
				len = sizeof(DATE_STRUCT);
				break;
			case SQL_C_TIME:
			case SQL_C_TYPE_TIME:
				len = sizeof(TIME_STRUCT);
				break;
			case SQL_C_TIMESTAMP:
			case SQL_C_TYPE_TIMESTAMP:
				len = sizeof(TIMESTAMP_STRUCT);
				break;
			case SQL_C_NUMERIC:
				len = sizeof(SQL_NUMERIC_STRUCT);
				break;
			default:
				len = tds_get_size_by_type(odbc_c_to_server_type(sql_src_type));
				break;
			}
			if (len < 0)
				/* TODO sure ? what happen to upper layer ?? */
				return SQL_ERROR;
			src += len * stmt->curr_param_row;
		}
	}

	/* if only output assume input is NULL */
	if (drec_ipd->sql_desc_parameter_type == SQL_PARAM_OUTPUT) {
		sql_len = SQL_NULL_DATA;
	} else {
		sql_len = odbc_get_param_len(drec_apd, drec_ipd);

		/* special case, MS ODBC handle conversion from "\0" to any to NULL, DBD::ODBC require it */
		if (src_type == SYBVARCHAR && sql_len == 1 && drec_ipd->sql_desc_parameter_type == SQL_PARAM_INPUT_OUTPUT
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
		assert(src != NULL);	/* TODO: check that SQLBindParameter::BufferLength is not zero for output parameters */
		len = strlen(src);
		break;
	case SQL_DEFAULT_PARAM:
	case SQL_DATA_AT_EXEC:
		/* TODO */
		return SQL_ERROR;
		break;
	default:
		len = sql_len;
		if (sql_len < 0) {
			/* test for SQL_C_CHAR/SQL_C_BINARY */
			switch (sql_src_type) {
			case SQL_C_CHAR:
			case SQL_C_BINARY:
				break;
			default:
				return SQL_ERROR;
			}
			len = SQL_LEN_DATA_AT_EXEC(sql_len);
			need_data = 1;

			/* dynamic length allowed only for BLOB fields */
			switch (drec_ipd->sql_desc_concise_type) {
			case SQL_LONGVARCHAR:
			case SQL_LONGVARBINARY:
				break;
			default:
				return SQL_ERROR;
			}
		}
	}

	/* allocate given space */
	if (!tds_alloc_param_data(info, curcol))
		return SQL_ERROR;

	if (need_data) {
		curcol->column_cur_size = 0;
		return SQL_NEED_DATA;
	}

	/* set null */
	assert(drec_ipd->sql_desc_parameter_type != SQL_PARAM_OUTPUT || sql_len == SQL_NULL_DATA);
	if (sql_len == SQL_NULL_DATA) {
		curcol->column_cur_size = -1;
		return TDS_SUCCEED;
	}

	/* convert special parameters (not libTDS compatible) */
	switch (src_type) {
	case SYBDATETIME:
		convert_datetime2server(drec_apd->sql_desc_concise_type, src, &dt);
		src = (char *) &dt;
		break;
	case SYBDECIMAL:
	case SYBNUMERIC:
		sql_num = (SQL_NUMERIC_STRUCT *) src;
		num.precision = sql_num->precision;
		num.scale = sql_num->scale;
		num.array[0] = sql_num->sign ^ 1;
		/* test precision so client do not crash our library */
		if (num.precision <= 0 || num.precision > 38 || num.scale > num.precision)
			/* TODO add proper error */
			return SQL_ERROR;
		i = tds_numeric_bytes_per_prec[num.precision];
		memcpy(num.array + 1, sql_num->val, i - 1);
		tds_swap_bytes(num.array + 1, i - 1);
		if (i < sizeof(num.array))
			memset(num.array + i, 0, sizeof(num.array) - i);
		src = (char *) &num;
		break;
		/* TODO intervals */
	}

	dest = curcol->column_data;
	switch ((TDS_SERVER_TYPE) dest_type) {
	case SYBCHAR:
	case SYBVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
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
	case SYBTEXT:
	case SYBLONGBINARY:
	case SYBIMAGE:
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, dest_type, &ores);
		if (res < 0)
			return SQL_ERROR;
		blob = (TDSBLOB *) dest;
		if (blob->textvalue)
			free(blob->textvalue);
		blob->textvalue = ores.ib;
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		((TDS_NUMERIC *) dest)->precision = drec_ipd->sql_desc_precision;
		((TDS_NUMERIC *) dest)->scale = drec_ipd->sql_desc_scale;
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
		res = tds_convert(dbc->env->tds_ctx, src_type, src, len, dest_type, (CONV_RESULT*) dest);
		break;
	default:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case SYBNVARCHAR:
	case SYBNTEXT:
	case SYBVOID:
	case SYBVARIANT:
		/* TODO ODBC 3.5 */
		assert(0);
		res = -1;
		break;
	}

	if (res < 0)
		return SQL_ERROR;

	curcol->column_cur_size = res;

	return SQL_SUCCESS;
}
