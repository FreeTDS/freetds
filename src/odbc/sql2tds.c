/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#include "tds.h"
#include "tdsodbc.h"
#include "tdsconvert.h"
#include "sql2tds.h"
#include "odbc_util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: sql2tds.c,v 1.28 2003-12-07 13:20:20 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static TDS_INT
convert_datetime2server(int bindtype, const void *src, TDS_DATETIME * dt)
{
	struct tds_time src_tm;
	unsigned int dt_time;
	TDS_INT dt_days;
	int i;

	const DATE_STRUCT *src_date = (const DATE_STRUCT *) src;
	const TIME_STRUCT *src_time = (const TIME_STRUCT *) src;
	const TIMESTAMP_STRUCT *src_timestamp = (const TIMESTAMP_STRUCT *) src;

	memset(&src_tm, 0, sizeof(src_tm));

	switch (bindtype) {
	case SQL_C_DATE:
	case SQL_C_TYPE_DATE:
		src_tm.tm_year = src_date->year - 1900;
		src_tm.tm_mon = src_date->month - 1;
		src_tm.tm_mday = src_date->day;
		break;
	case SQL_C_TIME:
	case SQL_C_TYPE_TIME:
		src_tm.tm_hour = src_time->hour;
		src_tm.tm_min = src_time->minute;
		src_tm.tm_sec = src_time->second;
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
	i = (src_tm.tm_mon - 13) / 12;
	dt_days = 1461 * (src_tm.tm_year + 300 + i) / 4 +
		(367 * (src_tm.tm_mon - 1 - 12 * i)) / 12 - (3 * ((src_tm.tm_year + 400 + i) / 100)) / 4 + src_tm.tm_mday - 109544;

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
sql2tds(TDS_DBC * dbc, struct _drecord *drec_ipd, struct _drecord *drec_apd, TDSPARAMINFO * info, int nparam)
{
	int dest_type, src_type, res;
	CONV_RESULT ores;
	TDSBLOBINFO *blob_info;
	char *src;
	unsigned char *dest;
	TDSCOLINFO *curcol = info->columns[nparam];
	int len;
	TDS_DATETIME dt;
	TDS_NUMERIC num;
	SQL_NUMERIC_STRUCT *sql_num;
	SQLINTEGER sql_len;
	int need_data = 0, i;

	/* TODO procedure, macro ?? see prepare_query */
	sql_len = odbc_get_param_len(dbc->tds_socket, drec_apd, drec_ipd);

	/* TODO handle bindings of char like "{d '2002-11-12'}" */
	tdsdump_log(TDS_DBG_INFO2, "%s:%d type=%d\n", __FILE__, __LINE__, drec_ipd->sql_desc_concise_type);

	/* what type to convert ? */
	dest_type = odbc_sql_to_server_type(dbc->tds_socket, drec_ipd->sql_desc_concise_type);
	if (dest_type == TDS_FAIL)
		return SQL_ERROR;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* TODO what happen for unicode types ?? */
	tds_set_param_type(dbc->tds_socket, curcol, dest_type);
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = drec_ipd->sql_desc_precision;
		curcol->column_scale = drec_ipd->sql_desc_scale;
	}

	/* FIXME len is source len while column refers to destination !!! */
	len = curcol->column_size;
	if (drec_ipd->sql_desc_parameter_type != SQL_PARAM_INPUT)
		curcol->column_output = 1;
	if (curcol->column_varint_size != 0) {
		switch (sql_len) {
		case SQL_NULL_DATA:
			len = 0;
			break;
		case SQL_NTS:
			len = strlen(drec_apd->sql_desc_data_ptr);
			break;
		case SQL_DEFAULT_PARAM:
		case SQL_DATA_AT_EXEC:
			/* TODO */
			return SQL_ERROR;
			break;
		default:
			if (sql_len < 0) {
				/* test for SQL_CHAR/SQL_BINARY */
				switch (drec_apd->sql_desc_concise_type) {
				case SQL_C_CHAR:
				case SQL_C_BINARY:
					break;
				case SQL_C_DEFAULT:
					switch (drec_ipd->sql_desc_concise_type) {
					case SQL_CHAR:
					case SQL_VARCHAR:
					case SQL_LONGVARCHAR:
					case SQL_BINARY:
					case SQL_VARBINARY:
					case SQL_LONGVARBINARY:
						break;
					default:
						return SQL_ERROR;
					}
					break;
				default:
					return SQL_ERROR;
				}
				len = SQL_LEN_DATA_AT_EXEC(sql_len);
				need_data = 1;
			} else {
				len = sql_len;
			}

		}
		curcol->column_cur_size = curcol->column_size = len;
		if (drec_ipd->sql_desc_parameter_type != SQL_PARAM_INPUT)
			curcol->column_size = drec_apd->sql_desc_octet_length;
	} else {
		/* TODO only a trick... */
		if (curcol->column_varint_size == 0)
			tds_set_param_type(dbc->tds_socket, curcol, tds_get_null_type(dest_type));
	}
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* allocate given space */
	if (!tds_alloc_param_row(info, curcol))
		return SQL_ERROR;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* test type */
	/* TODO test intervals */
	src_type = drec_apd->sql_desc_concise_type;
	if (src_type == SQL_C_DEFAULT)
		src_type = odbc_sql_to_c_type_default(drec_ipd->sql_desc_concise_type);
	src_type = odbc_c_to_server_type(src_type);
	if (src_type == TDS_FAIL)
		return SQL_ERROR;

	if (need_data) {
		curcol->column_cur_size = 0;
		return SQL_NEED_DATA;
	}

	/* set null */
	if (sql_len == SQL_NULL_DATA || drec_ipd->sql_desc_parameter_type == SQL_PARAM_OUTPUT) {
		curcol->column_cur_size = 0;
		tds_set_null(info->current_row, nparam);
		return TDS_SUCCEED;
	}
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* convert special parameters (not libTDS compatible) */
	src = drec_apd->sql_desc_data_ptr;
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
		/* TODO test precision so client do not crash our library ?? */
		i = tds_numeric_bytes_per_prec[num.precision];
		memcpy(num.array + 1, sql_num->val, i);
		tds_swap_bytes(num.array + 1, i);
		++i;
		if (i < sizeof(num.array))
			memset(num.array + i, 0, sizeof(num.array) - i);
		src = (char *) &num;

		/* set output */
		/* TODO use descriptors informations ?? */
		ores.n.precision = 18;
		ores.n.scale = 2;
		break;
		/* TODO intervals */
	}

	res = tds_convert(dbc->env->tds_ctx, src_type, src, len, dest_type, &ores);
	if (res < 0)
		return SQL_ERROR;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* truncate ?? */
	/* TODO what happen for blobs ?? */
	if (res > curcol->column_size)
		res = curcol->column_size;

	/* free allocated memory */
	dest = &info->current_row[curcol->column_offset];
	switch ((TDS_SERVER_TYPE) dest_type) {
	case SYBCHAR:
	case SYBVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
		memcpy(&info->current_row[curcol->column_offset], ores.c, res);
		free(ores.c);
		break;
	case SYBTEXT:
		blob_info = (TDSBLOBINFO *) dest;
		if (blob_info->textvalue)
			free(blob_info->textvalue);
		blob_info->textvalue = ores.c;
		break;
	case SYBBINARY:
	case SYBVARBINARY:
	case XSYBBINARY:
	case XSYBVARBINARY:
		memcpy(&info->current_row[curcol->column_offset], ores.ib, res);
		free(ores.ib);
		break;
	case SYBLONGBINARY:
	case SYBIMAGE:
		blob_info = (TDSBLOBINFO *) dest;
		if (blob_info->textvalue)
			free(blob_info->textvalue);
		blob_info->textvalue = ores.ib;
		break;
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
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
	case SYBUNIQUE:
		memcpy(&info->current_row[curcol->column_offset], &ores, res);
		break;
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case SYBNVARCHAR:
	case SYBNTEXT:
	case SYBVOID:
	case SYBVARIANT:
		/* TODO ODBC 3.5 */
		assert(0);
		break;
	}

	return SQL_SUCCESS;
}
