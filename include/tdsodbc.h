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

#ifndef _sql_h_
#define _sql_h_

#include <tds.h>

#ifdef UNIXODBC
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>
#else
#include "isql.h"
#include "isqlext.h"
#endif

#ifndef SQLULEN
#define SQLULEN SQLUINTEGER
#endif
#ifndef SQLLEN
#define SQLLEN SQLINTEGER
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

static char rcsid_sql_h[] = "$Id: tdsodbc.h,v 1.34 2003-07-28 12:30:10 freddy77 Exp $";
static void *no_unused_sql_h_warn[] = { rcsid_sql_h, no_unused_sql_h_warn };

struct _sql_error
{
	const char *msg;
	char state2[6];
	char state3[6];
	TDS_UINT native;
	char *server;
	int linenum;
	int msgstate;
};

struct _sql_errors
{
	int num_errors;
	struct _sql_error *errs;
};

#define ODBC_RETURN(handle, rc)       { return (handle->lastrc = (rc)); }

/** reset errors */
void odbc_errs_reset(struct _sql_errors *errs);

/** add an error to list */
void odbc_errs_add(struct _sql_errors *errs, TDS_UINT native, const char *sqlstate, const char *msg, const char *server);

/** Add an error to list. This functions is for error that came from server */
void odbc_errs_add_rdbms(struct _sql_errors *errs, TDS_UINT native, const char *sqlstate, const char *msg, int linenum, int msgstate, const char *server);

struct _heattr
{
	SQLUINTEGER attr_connection_pooling;
	SQLUINTEGER attr_cp_match;
	SQLINTEGER attr_odbc_version;
	SQLINTEGER attr_output_nts;
};

struct _hchk
{
	SQLSMALLINT htype;      /* do not reorder this field */
};

struct _henv
{
	SQLSMALLINT htype;      /* do not reorder this field */
	TDSCONTEXT *tds_ctx;
	struct _sql_errors errs;
	struct _heattr attr;
	SQLRETURN lastrc;
};

struct _hcattr
{
	SQLUINTEGER attr_access_mode;
	SQLUINTEGER attr_async_enable;
	SQLUINTEGER attr_auto_ipd;
	SQLUINTEGER attr_autocommit;
	SQLUINTEGER attr_connection_dead;
	SQLUINTEGER attr_connection_timeout;
	DSTR attr_current_catalog;
	SQLUINTEGER attr_login_timeout;
	SQLUINTEGER attr_metadata_id;
	SQLUINTEGER attr_odbc_cursors;
	SQLUINTEGER attr_packet_size;
	SQLHWND attr_quite_mode;
	/* DM only */
	/* SQLUINTEGER attr_trace;
	 * SQLCHAR *attr_tracefile; */
	DSTR attr_translate_lib;
	SQLUINTEGER attr_translate_option;
	SQLUINTEGER attr_txn_isolation;
};

struct _hstmt;
struct _hdbc
{
	SQLSMALLINT htype;      /* do not reorder this field */
	struct _henv *henv;
	TDSSOCKET *tds_socket;
	DSTR server;	/* aka Instance */
	/** statement executing */
	struct _hstmt *current_statement;
	struct _sql_errors errs;
	struct _hcattr attr;
	SQLRETURN lastrc;
};

struct _hstmt
{
	SQLSMALLINT htype;      /* do not reorder this field */
	struct _hdbc *hdbc;
	char *query;
	/* begin prepared query stuff */
	char *prepared_query;
	/** point inside prepared_query, position to continue processing (read) */
	char *prepared_query_s;
	/** point inside query, position to continue processing (write) */
	char *prepared_query_d;
	int prepared_query_need_bytes;
	int prepared_query_param_num;
	int prepared_query_is_func;
	int prepared_query_is_rpc;
	/* end prepared query stuff */
	struct _sql_bind_info *bind_head;
	struct _sql_param_info *param_head;
	/** number of parameter in current query */
	unsigned int param_count;
	int row;
	TDSDYNAMIC *dyn;	/* FIXME check if freed */
	struct _sql_errors errs;
	char ard, ird, apd, ipd;
	SQLRETURN lastrc;
};

struct _sql_param_info
{
	int param_number;
	int param_type;
	int param_bindtype;
	int param_sqltype;
	char *varaddr;
	int param_bindlen;
	SQLINTEGER *param_lenbind;
	/** this parameter is used if provided param_lenbind is NULL */
	SQLINTEGER param_inlen;
	struct _sql_param_info *next;
};

struct _sql_bind_info
{
	int column_number;
	int column_bindtype;
	int column_bindlen;
	char *varaddr;
	char *column_lenbind;
	struct _sql_bind_info *next;
};

typedef struct _henv TDS_ENV;
typedef struct _hdbc TDS_DBC;
typedef struct _hstmt TDS_STMT;
typedef struct _hchk TDS_CHK;

#define IS_HENV(x) (((TDS_CHK *)x)->htype == SQL_HANDLE_ENV)
#define IS_HDBC(x) (((TDS_CHK *)x)->htype == SQL_HANDLE_DBC)
#define IS_HSTMT(x) (((TDS_CHK *)x)->htype == SQL_HANDLE_STMT)
#define IS_HDESC(x) (((TDS_CHK *)x)->htype == SQL_HANDLE_DESC)

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
