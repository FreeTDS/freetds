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

static char rcsid_sql_h[] = "$Id: tdsodbc.h,v 1.39 2003-08-01 15:16:30 freddy77 Exp $";
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
void odbc_errs_add(struct _sql_errors *errs, const char *sqlstate, const char *msg, const char *server);

/** Add an error to list. This functions is for error that came from server */
void odbc_errs_add_rdbms(struct _sql_errors *errs, TDS_UINT native, const char *sqlstate, const char *msg, int linenum,
			 int msgstate, const char *server);

struct _dheader
{
	SQLSMALLINT sql_desc_alloc_type;
	SQLINTEGER sql_desc_bind_type;
	SQLUINTEGER sql_desc_array_size;
	SQLSMALLINT sql_desc_count;
	SQLUSMALLINT *sql_desc_array_status_ptr;
	SQLUINTEGER *sql_desc_rows_processed_ptr;
	SQLINTEGER *sql_desc_bind_offset_ptr;
};

struct _drecord
{
	SQLUINTEGER sql_desc_auto_unique_value;
	SQLCHAR *sql_desc_base_column_name;
	SQLCHAR *sql_desc_base_table_name;
	SQLINTEGER sql_desc_case_sensitive;
	SQLCHAR *sql_desc_catalog_name;
	SQLSMALLINT sql_desc_concise_type;
	SQLPOINTER *sql_desc_data_ptr;
	SQLSMALLINT sql_desc_datetime_interval_code;
	SQLINTEGER sql_desc_datetime_interval_precision;
	SQLINTEGER sql_desc_display_size;
	SQLSMALLINT sql_desc_fixed_prec_scale;
	SQLINTEGER *sql_desc_indicator_ptr;
	SQLCHAR *sql_desc_label;
	SQLUINTEGER sql_desc_length;
	SQLCHAR *sql_desc_literal_prefix;
	SQLCHAR *sql_desc_literal_suffix;
	SQLCHAR *sql_desc_local_type_name;
	SQLCHAR *sql_desc_name;
	SQLSMALLINT sql_desc_nullable;
	SQLINTEGER sql_desc_num_prec_radix;
	SQLINTEGER sql_desc_octet_length;
	SQLINTEGER *sql_desc_octet_length_ptr;
	SQLSMALLINT sql_desc_parameter_type;
	SQLSMALLINT sql_desc_precision;
	SQLSMALLINT sql_desc_rowver;
	SQLSMALLINT sql_desc_scale;
	SQLCHAR *sql_desc_schema_name;
	SQLSMALLINT sql_desc_searchable;
	SQLCHAR *sql_desc_table_name;
	SQLSMALLINT sql_desc_type;
	SQLCHAR *sql_desc_type_name;
	SQLSMALLINT sql_desc_unnamed;
	SQLSMALLINT sql_desc_unsigned;
	SQLSMALLINT sql_desc_updatable;
};

struct _hdesc
{
	SQLSMALLINT htype;	/* do not reorder this field */
	int type;
	SQLHDESC parent;
	struct _dheader header;
	struct _drecord *records;
	struct _sql_errors errs;
	SQLRETURN lastrc;
};

typedef struct _hdesc IRD;
typedef struct _hdesc IPD;
typedef struct _hdesc ARD;
typedef struct _hdesc APD;
typedef struct _hdesc TDS_DESC;

#define DESC_IRD	1
#define DESC_IPD	2
#define DESC_ARD	3
#define DESC_APD	4

struct _heattr
{
	SQLUINTEGER attr_connection_pooling;
	SQLUINTEGER attr_cp_match;
	SQLINTEGER attr_odbc_version;
	SQLINTEGER attr_output_nts;
};

struct _hchk
{
	SQLSMALLINT htype;	/* do not reorder this field */
};

struct _henv
{
	SQLSMALLINT htype;	/* do not reorder this field */
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
#ifdef TDS_NO_DM
	SQLUINTEGER attr_trace;
	DSTR attr_tracefile;
#endif
	DSTR attr_translate_lib;
	SQLUINTEGER attr_translate_option;
	SQLUINTEGER attr_txn_isolation;
};

struct _hstmt;
struct _hdbc
{
	SQLSMALLINT htype;	/* do not reorder this field */
	struct _henv *henv;
	TDSSOCKET *tds_socket;
	DSTR dsn;
	DSTR server;		/* aka Instance */
	/** statement executing */
	struct _hstmt *current_statement;
	struct _sql_errors errs;
	struct _hcattr attr;
	SQLRETURN lastrc;
};

struct _hstmt
{
	SQLSMALLINT htype;	/* do not reorder this field */
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

TDS_DESC *desc_alloc(SQLHDESC parent, int desc_type, int alloc_type);
SQLRETURN desc_free(TDS_DESC * desc);
SQLRETURN desc_alloc_records(TDS_DESC * desc, unsigned count);
SQLRETURN desc_free_records(TDS_DESC * desc);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
