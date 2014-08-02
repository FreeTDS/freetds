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

#  define TDS_DEFINE_FUNCS(name) \
	const TDS_FUNCS tds_ ## name ## _funcs = { \
		TDS_COMMON_FUNCS(name), \
		data_ ## name ## _server_to_sql_type \
	}
TDS_DEFINE_FUNCS(generic);
TDS_DEFINE_FUNCS(numeric);
TDS_DEFINE_FUNCS(variant);
TDS_DEFINE_FUNCS(msdatetime);
TDS_DEFINE_FUNCS(clrudt);
