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

#ifndef ODBC_UTIL_h
#define ODBC_UTIL_h

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if defined(UNIXODBC) || defined(TDS_NO_DM)
#include <sql.h>
#include <sqlext.h>
#else /* iODBC */
#include "isql.h"
#include "isqlext.h"
#endif

/* $Id: odbc_util.h,v 1.29 2004-12-02 11:29:43 freddy77 Exp $ */

int odbc_set_stmt_query(struct _hstmt *stmt, const char *sql, int sql_len);
int odbc_set_stmt_prepared_query(struct _hstmt *stmt, const char *sql, int sql_len);
void odbc_set_return_status(struct _hstmt *stmt);
void odbc_set_return_params(struct _hstmt *stmt);

SQLSMALLINT odbc_server_to_sql_type(int col_type, int col_size);
int odbc_sql_to_c_type_default(int sql_type);
int odbc_sql_to_server_type(TDSSOCKET * tds, int sql_type);
int odbc_c_to_server_type(int c_type);

void odbc_set_sql_type_info(TDSCOLUMN * col, int odbc_ver, struct _drecord *drec);
SQLINTEGER odbc_sql_to_displaysize(int sqltype, int column_size, int column_prec);
int odbc_get_string_size(int size, SQLCHAR * str);
void odbc_rdbms_version(TDSSOCKET * tds_socket, char *pversion_string);
SQLINTEGER odbc_get_param_len(TDSSOCKET * tds, const struct _drecord *drec_apd, const struct _drecord *drec_ipd);

SQLRETURN odbc_set_string(SQLPOINTER buffer, SQLSMALLINT cbBuffer, SQLSMALLINT FAR * pcbBuffer, const char *s, int len);
SQLRETURN odbc_set_string_i(SQLPOINTER buffer, SQLINTEGER cbBuffer, SQLINTEGER FAR * pcbBuffer, const char *s, int len);

SQLSMALLINT odbc_get_concise_sql_type(SQLSMALLINT type, SQLSMALLINT interval);
SQLRETURN odbc_set_concise_sql_type(SQLSMALLINT concise_type, struct _drecord *drec, int check_only);
SQLSMALLINT odbc_get_concise_c_type(SQLSMALLINT type, SQLSMALLINT interval);
SQLRETURN odbc_set_concise_c_type(SQLSMALLINT concise_type, struct _drecord *drec, int check_only);

#endif
