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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsodbc.h"
#include "odbc_util.h"
#include "convert_tds2sql.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: odbc_util.c,v 1.26 2003-04-29 19:37:15 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/**
 * \ingroup odbc_api
 * \defgroup odbc_util ODBC utility
 * Functions called within \c ODBC driver.
 */

/**
 * \addtogroup odbc_util
 *  \@{ 
 */

int
odbc_set_stmt_query(TDS_STMT * stmt, const char *sql, int sql_len)
{
	if (sql_len == SQL_NTS)
		sql_len = strlen(sql);
	else if (sql_len <= 0)
		return SQL_ERROR;

	if (stmt->prepared_query) {
		free(stmt->prepared_query);
		stmt->prepared_query = NULL;
	}

	if (stmt->query)
		free(stmt->query);

	stmt->query = (char *) malloc(sql_len + 1);
	if (!stmt->query)
		return SQL_ERROR;

	if (sql) {
		memcpy(stmt->query, sql, sql_len);
		stmt->query[sql_len] = 0;
	} else {
		stmt->query[0] = 0;
	}

	return SQL_SUCCESS;
}


int
odbc_set_stmt_prepared_query(TDS_STMT * stmt, const char *sql, int sql_len)
{
	if (sql_len == SQL_NTS)
		sql_len = strlen(sql);
	else if (sql_len <= 0)
		return SQL_ERROR;

	if (stmt->query) {
		free(stmt->query);
		stmt->query = NULL;
	}

	if (stmt->prepared_query)
		free(stmt->prepared_query);

	stmt->prepared_query = (char *) malloc(sql_len + 1);
	if (!stmt->prepared_query)
		return SQL_ERROR;

	if (sql) {
		memcpy(stmt->prepared_query, sql, sql_len);
		stmt->prepared_query[sql_len] = 0;
	} else {
		stmt->prepared_query[0] = 0;
	}

	return SQL_SUCCESS;
}


void
odbc_set_return_status(struct _hstmt *stmt)
{
	TDSSOCKET *tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	TDSCONTEXT *context = stmt->hdbc->henv->tds_ctx;

#if 0
	TDSLOCALE *locale = context->locale;
#endif

	if (stmt->prepared_query_is_func && tds->has_status) {
		struct _sql_param_info *param;

		param = odbc_find_param(stmt, 1);
		if (param) {
			int len = convert_tds2sql(context,
						  SYBINT4,
						  (TDS_CHAR *) & tds->ret_status,
						  sizeof(TDS_INT),
						  param->param_sqltype,
						  param->varaddr,
						  param->param_bindlen);

			if (TDS_FAIL == len)
				return /* SQL_ERROR */ ;
			*param->param_lenbind = len;
		}
	}

}


struct _sql_param_info *
odbc_find_param(struct _hstmt *stmt, int param_num)
{
	struct _sql_param_info *cur;

	/* find parameter number n */
	cur = stmt->param_head;
	while (cur) {
		if (cur->param_number == param_num)
			return cur;
		cur = cur->next;
	}
	return NULL;
}


int
odbc_get_string_size(int size, SQLCHAR * str)
{
	if (!str) {
		return 0;
	}
	if (size == SQL_NTS) {
		return strlen((const char *) str);
	} else {
		return size;
	}
}

/**
 * Convert type from database to ODBC
 */
SQLSMALLINT
odbc_tds_to_sql_type(int col_type, int col_size, int odbc_ver)
{
	/* FIXME finish */
	switch (col_type) {
	case XSYBCHAR:
	case SYBCHAR:
		return SQL_CHAR;
	case XSYBVARCHAR:
	case SYBVARCHAR:
		return SQL_VARCHAR;
	case SYBTEXT:
		return SQL_LONGVARCHAR;
	case SYBBIT:
	case SYBBITN:
		return SQL_BIT;
#if (ODBCVER >= 0x0300)
	case SYBINT8:
		/* TODO return numeric for odbc2 and convert bigint to numeric */
		return SQL_BIGINT;
#endif
	case SYBINT4:
		return SQL_INTEGER;
	case SYBINT2:
		return SQL_SMALLINT;
	case SYBINT1:
		return SQL_TINYINT;
	case SYBINTN:
		switch (col_size) {
		case 1:
			return SQL_TINYINT;
		case 2:
			return SQL_SMALLINT;
		case 4:
			return SQL_INTEGER;
#if (ODBCVER >= 0x0300)
		case 8:
			return SQL_BIGINT;
#endif
		}
		break;
	case SYBREAL:
		return SQL_REAL;
	case SYBFLT8:
		return SQL_DOUBLE;
	case SYBFLTN:
		switch (col_size) {
		case 4:
			return SQL_REAL;
		case 8:
			return SQL_DOUBLE;
		}
		break;
	case SYBMONEY:
		return SQL_DOUBLE;
	case SYBMONEY4:
		return SQL_DOUBLE;
	case SYBMONEYN:
		break;
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
#if (ODBCVER >= 0x0300)
		if (odbc_ver == 3)
			return SQL_TYPE_TIMESTAMP;
#endif
		return SQL_TIMESTAMP;
	case SYBBINARY:
		return SQL_BINARY;
	case SYBIMAGE:
		return SQL_LONGVARBINARY;
	case SYBVARBINARY:
		return SQL_VARBINARY;
	case SYBNUMERIC:
	case SYBDECIMAL:
		return SQL_NUMERIC;
	case SYBNTEXT:
	case SYBVOID:
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
		break;
#if (ODBCVER >= 0x0300)
	case SYBUNIQUE:
		return SQL_GUID;
	case SYBVARIANT:
		break;
#endif
	}
	return SQL_UNKNOWN_TYPE;
}

int
odbc_sql_to_c_type_default(int sql_type)
{

	switch (sql_type) {

	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
		/* FIXME why ? */
	case SQL_DECIMAL:
	case SQL_NUMERIC:
	case SQL_GUID:
		return SQL_C_CHAR;
	case SQL_BIT:
		return SQL_C_BIT;
	case SQL_TINYINT:
		return SQL_C_UTINYINT;
	case SQL_SMALLINT:
		return SQL_C_SSHORT;
	case SQL_INTEGER:
		return SQL_C_SLONG;
	case SQL_BIGINT:
		return SQL_C_SBIGINT;
	case SQL_REAL:
		return SQL_C_FLOAT;
	case SQL_FLOAT:
	case SQL_DOUBLE:
		return SQL_C_DOUBLE;
	case SQL_DATE:
		return SQL_C_DATE;
	case SQL_TIME:
		return SQL_C_TIME;
	case SQL_TIMESTAMP:
		return SQL_C_TIMESTAMP;
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		return SQL_C_BINARY;
		/* TODO */
	case SQL_TYPE_DATE:
	case SQL_TYPE_TIME:
	case SQL_TYPE_TIMESTAMP:
		/* TODO interval types */
	default:
		return 0;
	}
}

int
odbc_sql_to_server_type(TDSSOCKET * tds, int sql_type)
{

	switch (sql_type) {

	case SQL_CHAR:
		if (IS_TDS7_PLUS(tds))
			return XSYBCHAR;
		return SYBCHAR;
	case SQL_VARCHAR:
		if (IS_TDS7_PLUS(tds))
			return XSYBVARCHAR;
		return SYBVARCHAR;
	case SQL_LONGVARCHAR:
		return SYBTEXT;
	case SQL_DECIMAL:
		return SYBDECIMAL;
	case SQL_NUMERIC:
		return SYBNUMERIC;
	case SQL_GUID:
		if (IS_TDS7_PLUS(tds))
			return SYBUNIQUE;
		return 0;
	case SQL_BIT:
		return SYBBITN;
	case SQL_TINYINT:
		return SYBINT1;
	case SQL_SMALLINT:
		return SYBINT2;
	case SQL_INTEGER:
		return SYBINT4;
	case SQL_BIGINT:
		return SYBINT8;
	case SQL_REAL:
		return SYBREAL;
	case SQL_FLOAT:
	case SQL_DOUBLE:
		return SYBFLT8;
	case SQL_DATE:
	case SQL_TIME:
	case SQL_TIMESTAMP:
		return SYBDATETIME;
	case SQL_BINARY:
		if (IS_TDS7_PLUS(tds))
			return XSYBBINARY;
		return SYBBINARY;
	case SQL_VARBINARY:
		if (IS_TDS7_PLUS(tds))
			return XSYBVARBINARY;
		return SYBVARBINARY;
	case SQL_LONGVARBINARY:
		return SYBIMAGE;
		/* TODO */
	case SQL_TYPE_DATE:
	case SQL_TYPE_TIME:
	case SQL_TYPE_TIMESTAMP:
		/* TODO interval types */
	default:
		return 0;
	}
}

/**
 * Copy a string to client setting size according to ODBC convenction
 * @param buffer    client buffer
 * @param cbBuffer  client buffer size (in bytes)
 * @param pcbBuffer pointer to SQLSMALLINT to hold string size
 * @param s         string to copy
 * @param len       len of string to copy. <0 null terminated
 */
SQLRETURN
odbc_set_string(SQLPOINTER buffer, SQLSMALLINT cbBuffer, SQLSMALLINT FAR * pcbBuffer, const char *s, int len)
{
	SQLRETURN result = SQL_SUCCESS;

	if (len < 0)
		len = strlen(s);

	if (pcbBuffer)
		*pcbBuffer = len;
	if (len >= cbBuffer) {
		len = cbBuffer - 1;
		result = SQL_SUCCESS_WITH_INFO;
	}
	if (buffer && len >= 0) {
		strncpy((char *) buffer, s, len);
		((char *) buffer)[len] = 0;
	}
	return result;
}

/** \@} */
