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

/**
 * Convert type from database to ODBC
 */
static SQLSMALLINT
data_generic_server_to_sql_type(TDSCOLUMN *col)
{
	int col_type = col->on_server.column_type;
	int col_size = col->on_server.column_size;

	/* FIXME finish */
	switch (tds_get_conversion_type(col_type, col_size)) {
	case XSYBCHAR:
	case SYBCHAR:
		return SQL_CHAR;
	case XSYBVARCHAR:
	case SYBVARCHAR:
		return SQL_VARCHAR;
	case SYBTEXT:
		return SQL_LONGVARCHAR;
	case XSYBNCHAR:
		return SQL_WCHAR;
	/* TODO really sure ?? SYBNVARCHAR sybase only ?? */
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
		return SQL_WVARCHAR;
	case SYBNTEXT:
		return SQL_WLONGVARCHAR;
	case SYBBIT:
		return SQL_BIT;
#if (ODBCVER >= 0x0300)
	case SYBUINT8:
	case SYB5INT8:
	case SYBINT8:
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		return SQL_BIGINT;
#endif
	case SYBINT4:
	case SYBUINT4:
		return SQL_INTEGER;
	case SYBUINT2:
	case SYBINT2:
		return SQL_SMALLINT;
	case SYBUINT1:
	case SYBSINT1:
	case SYBINT1:
		return SQL_TINYINT;
	case SYBREAL:
		return SQL_REAL;
	case SYBFLT8:
		return SQL_DOUBLE;
	case SYBMONEY:
	case SYBMONEY4:
		return SQL_DECIMAL;
	case SYBDATETIME:
	case SYBDATETIME4:
#if (ODBCVER >= 0x0300)
		return SQL_TYPE_TIMESTAMP;
#else
		return SQL_TIMESTAMP;
#endif
	case XSYBBINARY:
	case SYBBINARY:
		return SQL_BINARY;
	case SYBLONGBINARY:
	case SYBIMAGE:
		return SQL_LONGVARBINARY;
	case XSYBVARBINARY:
	case SYBVARBINARY:
		return SQL_VARBINARY;
#if (ODBCVER >= 0x0300)
	case SYBUNIQUE:
#ifdef SQL_GUID
		return SQL_GUID;
#else
		return SQL_CHAR;
#endif
#endif
	case SYBMSXML:
		return SQL_CHAR;
		/*
		 * TODO what should I do with these types ??
		 * return other types can cause additional problems
		 */
	case SYBVOID:
	case SYBDATE:
	case SYBDATEN:
	case SYBINTERVAL:
	case SYBTIME:
	case SYBTIMEN:
	case SYBUNITEXT:
	case SYBXML:
	case SYBMSUDT:
		/* these types are handled by tds_get_conversion_type */
	case SYBINTN:
	case SYBBITN:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBUINTN:
		break;
	}
	return SQL_UNKNOWN_TYPE;
}

static SQLSMALLINT
data_msdatetime_server_to_sql_type(TDSCOLUMN *col)
{
	switch (col->on_server.column_type) {
	case SYBMSTIME:
		return SQL_SS_TIME2;
	case SYBMSDATE:
		return SQL_TYPE_DATE;
	case SYBMSDATETIMEOFFSET:
		return SQL_SS_TIMESTAMPOFFSET;
	case SYBMSDATETIME2:
		return SQL_TYPE_TIMESTAMP;
	}
	return SQL_UNKNOWN_TYPE;
}

static SQLSMALLINT
data_variant_server_to_sql_type(TDSCOLUMN *col)
{
	/* TODO support it */
	return SQL_UNKNOWN_TYPE;
}

static SQLSMALLINT
data_numeric_server_to_sql_type(TDSCOLUMN *col)
{
	return SQL_NUMERIC;
}

static SQLSMALLINT
data_clrudt_server_to_sql_type(TDSCOLUMN *col)
{
	return SQL_LONGVARBINARY;
}

static void
data_set_type_info(TDSCOLUMN * col, struct _drecord *drec, SQLINTEGER odbc_ver)
{
	const char *type;

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

	drec->sql_desc_unsigned = SQL_FALSE;
	drec->sql_desc_octet_length = drec->sql_desc_length = col->on_server.column_size;

	switch (tds_get_conversion_type(col->column_type, col->column_size)) {
	case XSYBCHAR:
	case SYBCHAR:
		if (col->on_server.column_type == XSYBNCHAR)
			SET_INFO2("nchar", "'", "'", col->on_server.column_size / 2);
		SET_INFO("char", "'", "'");
	case XSYBVARCHAR:
	case SYBVARCHAR:
		type = "varchar";
		if (col->on_server.column_type == SYBNVARCHAR || col->on_server.column_type == XSYBNVARCHAR) {
			drec->sql_desc_length = col->on_server.column_size / 2u;
			type = "nvarchar";
		}
		if (is_blob_col(col))
			drec->sql_desc_octet_length = drec->sql_desc_length =
				SQL_SS_LENGTH_UNLIMITED;
		SET_INFO(type, "'", "'");
	case SYBTEXT:
		if (col->on_server.column_type == SYBNTEXT)
			SET_INFO2("ntext", "'", "'", col->on_server.column_size / 2);
		SET_INFO("text", "'", "'");
	case SYBBIT:
	case SYBBITN:
		drec->sql_desc_unsigned = SQL_TRUE;
		SET_INFO2("bit", "", "", 1);
#if (ODBCVER >= 0x0300)
	case SYBINT8:
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		SET_INFO2("bigint", "", "", 19);
#endif
	case SYBINT4:
		SET_INFO2("int", "", "", 10);
	case SYBINT2:
		SET_INFO2("smallint", "", "", 5);
	case SYBUINT1:
	case SYBINT1:
		drec->sql_desc_unsigned = SQL_TRUE;
		SET_INFO2("tinyint", "", "", 3);
#if (ODBCVER >= 0x0300)
	case SYBUINT8:
		drec->sql_desc_unsigned = SQL_TRUE;
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		SET_INFO2("unsigned bigint", "", "", 19);
#endif
	case SYBUINT4:
		drec->sql_desc_unsigned = SQL_TRUE;
		SET_INFO2("unsigned int", "", "", 10);
	case SYBUINT2:
		drec->sql_desc_unsigned = SQL_TRUE;
		SET_INFO2("unsigned smallint", "", "", 5);
	case SYBREAL:
		SET_INFO2("real", "", "", odbc_ver == SQL_OV_ODBC3 ? 24 : 7);
	case SYBFLT8:
		SET_INFO2("float", "", "", odbc_ver == SQL_OV_ODBC3 ? 53 : 15);
	case SYBMONEY:
		drec->sql_desc_octet_length = 21;
		SET_INFO2("money", "$", "", 19);
	case SYBMONEY4:
		drec->sql_desc_octet_length = 12;
		SET_INFO2("money", "$", "", 10);
	case SYBDATETIME:
		drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
		SET_INFO2("datetime", "'", "'", 23);
	case SYBDATETIME4:
		drec->sql_desc_octet_length = sizeof(TIMESTAMP_STRUCT);
		SET_INFO2("datetime", "'", "'", 16);
	case SYBBINARY:
		/* handle TIMESTAMP using usertype */
		if (col->column_usertype == 80)
			SET_INFO("timestamp", "0x", "");
		SET_INFO("binary", "0x", "");
	case SYBIMAGE:
		SET_INFO("image", "0x", "");
	case SYBVARBINARY:
		if (is_blob_col(col))
			drec->sql_desc_octet_length = drec->sql_desc_length =
				SQL_SS_LENGTH_UNLIMITED;
		SET_INFO("varbinary", "0x", "");
	case SYBNUMERIC:
		drec->sql_desc_octet_length = col->column_prec + 2;
		SET_INFO2("numeric", "", "", col->column_prec);
	case SYBDECIMAL:
		drec->sql_desc_octet_length = col->column_prec + 2;
		SET_INFO2("decimal", "", "", col->column_prec);
	case SYBINTN:
	case SYBDATETIMN:
	case SYBFLTN:
	case SYBMONEYN:
		assert(0);
	case SYBVOID:
	case SYBNTEXT:
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
		break;
#if (ODBCVER >= 0x0300)
	case SYBUNIQUE:
		/* FIXME for Sybase ?? */
		SET_INFO2("uniqueidentifier", "'", "'", 36);
	case SYBVARIANT:
		SET_INFO("sql_variant", "", "");
		break;
#endif
	case SYBMSDATETIMEOFFSET:
		SET_INFO2("datetimeoffset", "'", "'", col->column_prec + 27);
	case SYBMSDATETIME2:
		SET_INFO2("datetime2", "'", "'", col->column_prec + 20);
	case SYBMSTIME:
		SET_INFO2("time", "'", "'", col->column_prec + 9);
	case SYBMSDATE:
		SET_INFO2("date", "'", "'", 10);
	case SYBMSXML:
		SET_INFO("xml", "'", "'");
	}
	SET_INFO("", "", "");
#undef SET_INFO
#undef SET_INFO2
}

#  define TDS_DEFINE_FUNCS(name) \
	const TDS_FUNCS tds_ ## name ## _funcs = { \
		TDS_COMMON_FUNCS(name), \
		data_ ## name ## _server_to_sql_type, \
		data_set_type_info, \
	}
TDS_DEFINE_FUNCS(generic);
TDS_DEFINE_FUNCS(numeric);
TDS_DEFINE_FUNCS(variant);
TDS_DEFINE_FUNCS(msdatetime);
TDS_DEFINE_FUNCS(clrudt);
