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

static char rcsid_sql_h[] = "$Id: tdsodbc.h,v 1.30 2003-07-11 15:08:11 freddy77 Exp $";
static void *no_unused_sql_h_warn[] = { rcsid_sql_h, no_unused_sql_h_warn };

/* this is usually a const struct that store all errors */
struct _sql_error_struct
{
	const char *msg;
			 /**< default message */
	char state2[6];
			 /**< state for ODBC2 */
	char state3[6];
			 /**< state for ODBC3 */
};

struct _sql_error
{
	const struct _sql_error_struct *err;
	/* override error if specified */
	char *msg;
	char sqlstate[6];
	int msgstate;
	int msgnum;
	int linenum;
};

struct _sql_errors
{
	int num_errors;
	struct _sql_error *errs;
};

enum _sql_error_types
{
	ODBCERR_GENERIC,
	ODBCERR_NOTIMPLEMENTED,
	ODBCERR_MEMORY,
	ODBCERR_NODSN,
	ODBCERR_CONNECT,
	ODBCERR_INVALIDINDEX,
	ODBCERR_INVALIDTYPE,
	ODBCERR_INVALIDBUFFERLEN,
	ODBCERR_DATATRUNCATION,
	ODBCERR_NORESULT,
	ODBCERR_INVALIDOPTION
};

#define ODBC_RETURN(handle, rc)       { return (handle->lastrc = (rc)); }

/** reset errors */
void odbc_errs_reset(struct _sql_errors *errs);

/** add an error to list */
void odbc_errs_add(struct _sql_errors *errs, enum _sql_error_types err_type, const char *msg);

/** Add an error to list. This functions is for error that came from server */
void odbc_errs_add_rdbms(struct _sql_errors *errs, enum _sql_error_types err_type, char *msg, char *sqlstate,
			 int msgnum, unsigned short linenum, int msgstate);

struct _henv
{
	TDSCONTEXT *tds_ctx;
	struct _sql_errors errs;
	unsigned char odbc_ver;
	SQLRETURN lastrc;
};

struct _hstmt;
struct _hdbc
{
	struct _henv *henv;
	TDSSOCKET *tds_socket;
	/** statement executing */
	struct _hstmt *current_statement;
	/* spinellia@acm.org */
	/** 0 = OFF, 1 = ON, -1 = UNKNOWN */
	int autocommit_state;
	struct _sql_errors errs;
	
	DSTR current_database;
	SQLRETURN lastrc;
};

struct _hstmt
{
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

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
