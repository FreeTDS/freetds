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

#ifndef ODBC_UTIL_h
#define ODBC_UTIL_h

#include <config.h>
#ifdef UNIXODBC
#include <sql.h>
#include <sqlext.h>
#else
#include "isql.h"
#include "isqlext.h"
#endif

static char  rcsid_odbc_util_h [ ] =
         "$Id: odbc_util.h,v 1.1 2002-05-29 10:58:25 brianb Exp $";
static void *no_unused_odbc_util_h_warn[]={rcsid_odbc_util_h, 
                                         no_unused_odbc_util_h_warn};

int odbc_set_stmt_query(struct _hstmt *stmt, char *sql, int sql_len);
int odbc_set_stmt_prepared_query(struct _hstmt *stmt, char *sql, int sql_len);

SQLSMALLINT odbc_get_client_type(int col_type, int col_size);
int odbc_fix_literals(struct _hstmt *stmt);
int odbc_get_string_size(int size, char *str);

struct _sql_param_info * 
odbc_find_param(struct _hstmt *stmt, int param_num);


#endif
