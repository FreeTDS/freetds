/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2014  Frediano Ziglio
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>
#include <ctype.h>

#include <freetds/odbc.h>
#include <odbcss.h>

#define SET_INFO(type, prefix, suffix) do { \
	drec->sql_desc_literal_prefix = prefix; \
	drec->sql_desc_literal_suffix = suffix; \
	drec->sql_desc_type_name = type; \
	return; \
	} while(0)
#define SET_INFO2(type, prefix, suffix, len) do { \
	drec->sql_desc_length = (len); \
	SET_INFO(type, prefix, suffix); \
	} while(0)

static void
data_msdatetime_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	int decimals = col->column_prec ? col->column_prec + 1: 0;

	switch (col->on_server.column_type) {
	case SYBMSTIME:
		drec->sql_desc_octet_length = sizeof(SQL_SS_TIME2_STRUCT);
		drec->sql_desc_concise_type = SQL_SS_TIME2;
		/* always hh:mm:ss[.fff] */
		drec->sql_desc_display_size = 8 + decimals;
		SET_INFO2("time", "'", "'", 8 + decimals);
	case SYBMSDATE:
		drec->sql_desc_octet_length = sizeof(DATE_STRUCT);
		drec->sql_desc_concise_type = SQL_TYPE_DATE;
		/* always yyyy-mm-dd ?? */
		drec->sql_desc_display_size = 10;
		SET_INFO2("date", "'", "'", 10);
	case SYBMSDATETIMEOFFSET:
		drec->sql_desc_octet_length = sizeof(SQL_SS_TIMESTAMPOFFSET_STRUCT);
		drec->sql_desc_concise_type = SQL_SS_TIMESTAMPOFFSET;
		/* we always format using yyyy-mm-dd hh:mm:ss[.fff] +HH:MM, see convert_tds2sql.c */
		drec->sql_desc_display_size = 26 + decimals;
		SET_INFO2("datetimeoffset", "'", "'", 26 + decimals);
	case SYBMSDATETIME2:
		drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
		drec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
		drec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		/* we always format using yyyy-mm-dd hh:mm:ss[.fff], see convert_tds2sql.c */
		drec->sql_desc_display_size = 19 + decimals;
		SET_INFO2("datetime2", "'", "'", 19 + decimals);
	default:
		break;
	}
}

static void
data_variant_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	drec->sql_desc_concise_type = SQL_SS_VARIANT;
	drec->sql_desc_display_size = 8000;
	drec->sql_desc_octet_length = 0;
	SET_INFO2("sql_variant", "", "", 8000);
}

static void
data_numeric_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	const char *type_name =
		col->on_server.column_type == SYBNUMERIC ? "numeric" : "decimal";

	drec->sql_desc_concise_type = SQL_NUMERIC;
	drec->sql_desc_octet_length = col->column_prec + 2;
	drec->sql_desc_display_size = col->column_prec + 2;
	drec->sql_desc_num_prec_radix = 10;
	SET_INFO2(type_name, "", "", col->column_prec);
}

static void
data_clrudt_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	drec->sql_desc_concise_type = SQL_LONGVARBINARY;
	/* TODO ??? */
	drec->sql_desc_display_size = col->column_size * 2;
}

static void
data_sybbigtime_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	if (col->on_server.column_type == SYB5BIGTIME) {
		drec->sql_desc_concise_type = SQL_SS_TIME2;
		/* we always format using hh:mm:ss[.ffffff], see convert_tds2sql.c */
		drec->sql_desc_display_size = 15;
		drec->sql_desc_octet_length = sizeof(SQL_SS_TIME2_STRUCT);
		drec->sql_desc_precision = 6;
		drec->sql_desc_scale     = 6;
		drec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		SET_INFO2("bigtime", "'", "'", 15);
	}

	assert(col->on_server.column_type == SYB5BIGDATETIME);

	drec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
	drec->sql_desc_display_size = 26;
	drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
	drec->sql_desc_precision = 6;
	drec->sql_desc_scale     = 6;
	drec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
	SET_INFO2("bigdatetime", "'", "'", 26);
}

static void
data_generic_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	TDS_SERVER_TYPE col_type = col->on_server.column_type;
	int col_size = col->on_server.column_size;

	switch (tds_get_conversion_type(col_type, col_size)) {
	case XSYBNCHAR:
		drec->sql_desc_concise_type = SQL_WCHAR;
		drec->sql_desc_display_size = col->on_server.column_size / 2;
		SET_INFO2("nchar", "'", "'", col->on_server.column_size / 2);

	case XSYBCHAR:
	case SYBCHAR:
		drec->sql_desc_concise_type = SQL_CHAR;
		drec->sql_desc_display_size = col->on_server.column_size;
		SET_INFO("char", "'", "'");

	/* TODO really sure ?? SYBNVARCHAR sybase only ?? */
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
		drec->sql_desc_concise_type = SQL_WVARCHAR;
		drec->sql_desc_display_size = col->on_server.column_size / 2;
		drec->sql_desc_length = col->on_server.column_size / 2u;
		if (is_blob_col(col)) {
			drec->sql_desc_display_size = SQL_SS_LENGTH_UNLIMITED;
			drec->sql_desc_octet_length = drec->sql_desc_length =
				SQL_SS_LENGTH_UNLIMITED;
		}
		SET_INFO("nvarchar", "'", "'");

	case XSYBVARCHAR:
	case SYBVARCHAR:
		drec->sql_desc_concise_type = SQL_VARCHAR;
		drec->sql_desc_display_size = col->on_server.column_size;
		if (is_blob_col(col)) {
			drec->sql_desc_display_size = SQL_SS_LENGTH_UNLIMITED;
			drec->sql_desc_octet_length = drec->sql_desc_length =
				SQL_SS_LENGTH_UNLIMITED;
		}
		SET_INFO("varchar", "'", "'");

	case SYBNTEXT:
		drec->sql_desc_concise_type = SQL_WLONGVARCHAR;
		drec->sql_desc_display_size = col->on_server.column_size / 2;
		SET_INFO2("ntext", "'", "'", col->on_server.column_size / 2);

	case SYBTEXT:
		drec->sql_desc_concise_type = SQL_LONGVARCHAR;
		drec->sql_desc_display_size = col->on_server.column_size;
		SET_INFO("text", "'", "'");

	case SYBBIT:
	case SYBBITN:
		drec->sql_desc_concise_type = SQL_BIT;
		drec->sql_desc_display_size = 1;
		drec->sql_desc_unsigned = SQL_TRUE;
		SET_INFO2("bit", "", "", 1);

#if (ODBCVER >= 0x0300)
	case SYB5INT8:
	case SYBINT8:
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		drec->sql_desc_concise_type = SQL_BIGINT;
		drec->sql_desc_display_size = 20;
		SET_INFO2("bigint", "", "", 19);
#endif

	case SYBINT4:
		drec->sql_desc_concise_type = SQL_INTEGER;
		drec->sql_desc_display_size = 11;	/* -1000000000 */
		SET_INFO2("int", "", "", 10);

	case SYBINT2:
		drec->sql_desc_concise_type = SQL_SMALLINT;
		drec->sql_desc_display_size = 6;	/* -10000 */
		SET_INFO2("smallint", "", "", 5);

	case SYBUINT1:
	case SYBINT1:
		drec->sql_desc_unsigned = SQL_TRUE;
	case SYBSINT1: /* TODO not another type_name ?? */
		drec->sql_desc_concise_type = SQL_TINYINT;
		drec->sql_desc_display_size = 3;	/* 255 */
		SET_INFO2("tinyint", "", "", 3);

#if (ODBCVER >= 0x0300)
	case SYBUINT8:
		drec->sql_desc_unsigned = SQL_TRUE;
		drec->sql_desc_concise_type = SQL_BIGINT;
		drec->sql_desc_display_size = 20;
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		SET_INFO2("unsigned bigint", "", "", 20);
#endif

	case SYBUINT4:
		drec->sql_desc_unsigned = SQL_TRUE;
		drec->sql_desc_concise_type = SQL_INTEGER;
		drec->sql_desc_display_size = 10;
		SET_INFO2("unsigned int", "", "", 10);

	case SYBUINT2:
		drec->sql_desc_unsigned = SQL_TRUE;
		drec->sql_desc_concise_type = SQL_SMALLINT;
		drec->sql_desc_display_size = 5;	/* 65535 */
		SET_INFO2("unsigned smallint", "", "", 5);

	case SYBREAL:
		drec->sql_desc_concise_type = SQL_REAL;
		drec->sql_desc_display_size = 14;
		SET_INFO2("real", "", "", odbc_ver == SQL_OV_ODBC3 ? 24 : 7);

	case SYBFLT8:
		drec->sql_desc_concise_type = SQL_DOUBLE;
		drec->sql_desc_display_size = 24;	/* FIXME -- what should the correct size be? */
		SET_INFO2("float", "", "", odbc_ver == SQL_OV_ODBC3 ? 53 : 15);

	case SYBMONEY:
		/* TODO check money format returned by propretary ODBC, scale == 4 but we use 2 digits */
		drec->sql_desc_concise_type = SQL_DECIMAL;
		drec->sql_desc_octet_length = 21;
		drec->sql_desc_display_size = 21;
		drec->sql_desc_precision = 19;
		drec->sql_desc_scale     = 4;
		SET_INFO2("money", "$", "", 19);

	case SYBMONEY4:
		drec->sql_desc_concise_type = SQL_DECIMAL;
		drec->sql_desc_octet_length = 12;
		drec->sql_desc_display_size = 12;
		drec->sql_desc_precision = 10;
		drec->sql_desc_scale     = 4;
		SET_INFO2("money", "$", "", 10);

	case SYBDATETIME:
		drec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
		drec->sql_desc_display_size = 23;
		drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
		drec->sql_desc_precision = 3;
		drec->sql_desc_scale     = 3;
		drec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		SET_INFO2("datetime", "'", "'", 23);

	case SYBDATETIME4:
		drec->sql_desc_concise_type = SQL_TYPE_TIMESTAMP;
		/* TODO dependent on precision (decimal second digits) */
		/* we always format using yyyy-mm-dd hh:mm:ss[.fff], see convert_tds2sql.c */
		drec->sql_desc_display_size = 19;
		drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
		drec->sql_desc_datetime_interval_code = SQL_CODE_TIMESTAMP;
		SET_INFO2("datetime", "'", "'", 16);

	/* The following two types are just Sybase types but as mainly our ODBC
	 * driver is much more compatible with Windows use attributes similar
	 * to MS one. For instance Sybase ODBC returns TIME into a TIME_STRUCT
	 * however this truncate the precision to 0 as TIME does not have
	 * fraction of seconds. Also Sybase ODBC have different concepts for
	 * PRECISION for many types and making these 2 types compatibles with
	 * Sybase would break this driver compatibility.
	 */
	case SYBTIME:
		drec->sql_desc_concise_type = SQL_SS_TIME2;
		drec->sql_desc_octet_length = sizeof(SQL_SS_TIME2_STRUCT);
		/* we always format using hh:mm:ss[.fff], see convert_tds2sql.c */
		drec->sql_desc_display_size = 12;
		drec->sql_desc_precision = 3;
		drec->sql_desc_scale     = 3;
		SET_INFO2("time", "'", "'", 12);

	case SYBDATE:
		drec->sql_desc_octet_length = sizeof(DATE_STRUCT);
		drec->sql_desc_concise_type = SQL_TYPE_DATE;
		/* we always format using yyyy-mm-dd, see convert_tds2sql.c */
		drec->sql_desc_display_size = 10;
		SET_INFO2("date", "'", "'", 10);

	case XSYBBINARY:
	case SYBBINARY:
		drec->sql_desc_concise_type = SQL_BINARY;
		drec->sql_desc_display_size = col->column_size * 2;
		/* handle TIMESTAMP using usertype */
		if (col->column_usertype == 80)
			SET_INFO("timestamp", "0x", "");
		SET_INFO("binary", "0x", "");

	case SYBLONGBINARY:
	case SYBIMAGE:
		drec->sql_desc_concise_type = SQL_LONGVARBINARY;
		drec->sql_desc_display_size = col->column_size * 2;
		SET_INFO("image", "0x", "");

	case XSYBVARBINARY:
	case SYBVARBINARY:
		drec->sql_desc_concise_type = SQL_VARBINARY;
		drec->sql_desc_display_size = col->column_size * 2;
		if (is_blob_col(col)) {
			drec->sql_desc_display_size = SQL_SS_LENGTH_UNLIMITED;
			drec->sql_desc_octet_length = drec->sql_desc_length =
				SQL_SS_LENGTH_UNLIMITED;
		}
		SET_INFO("varbinary", "0x", "");

	case SYBINTN:
	case SYBDATETIMN:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBUINTN:
	case SYBTIMEN:
	case SYBDATEN:
		assert(0);

	case SYBVOID:
	case SYBINTERVAL:
	case SYBUNITEXT:
	case SYBXML:
	case SYBMSUDT:
		break;

#if (ODBCVER >= 0x0300)
	case SYBUNIQUE:
#ifdef SQL_GUID
		drec->sql_desc_concise_type = SQL_GUID;
#else
		drec->sql_desc_concise_type = SQL_CHAR;
#endif
		drec->sql_desc_display_size = 36;
		/* FIXME for Sybase ?? */
		SET_INFO2("uniqueidentifier", "'", "'", 36);
#endif

	case SYBMSXML:
		drec->sql_desc_concise_type = SQL_SS_XML;
		drec->sql_desc_display_size = SQL_SS_LENGTH_UNLIMITED;
		drec->sql_desc_octet_length = drec->sql_desc_length =
			SQL_SS_LENGTH_UNLIMITED;
		SET_INFO("xml", "'", "'");
	}
	SET_INFO("", "", "");
}

static void
data_invalid_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
}

void
odbc_set_sql_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	drec->sql_desc_precision = col->column_prec;
	drec->sql_desc_scale     = col->column_scale;
	drec->sql_desc_unsigned = SQL_FALSE;
	drec->sql_desc_octet_length = drec->sql_desc_length = col->on_server.column_size;
	drec->sql_desc_num_prec_radix = 0;
	drec->sql_desc_datetime_interval_code = 0;

	((TDS_FUNCS *) col->funcs)->set_type_info(col, drec, odbc_ver);

	drec->sql_desc_type = drec->sql_desc_concise_type;
	if (drec->sql_desc_concise_type == SQL_TYPE_TIMESTAMP)
		drec->sql_desc_type = SQL_DATETIME;
}

#  define TDS_DEFINE_FUNCS(name) \
	const TDS_FUNCS tds_ ## name ## _funcs = { \
		TDS_COMMON_FUNCS(name), \
		data_ ## name ## _set_type_info, \
	}
TDS_DEFINE_FUNCS(invalid);
TDS_DEFINE_FUNCS(generic);
TDS_DEFINE_FUNCS(numeric);
TDS_DEFINE_FUNCS(variant);
TDS_DEFINE_FUNCS(msdatetime);
TDS_DEFINE_FUNCS(clrudt);
TDS_DEFINE_FUNCS(sybbigtime);
