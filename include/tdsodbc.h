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

#ifndef _sql_h_
#define _sql_h_

#include <tds.h>

#ifdef UNIXODBC
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>
#elif defined(TDS_NO_DM)
#include <sql.h>
#include <sqlext.h>
#else /* IODBC */
#include <isql.h>
#include <isqlext.h>
#ifdef HAVE_IODBCINST_H
#include <iodbcinst.h>
#endif /* HAVE_IODBCINST_H */
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

static char rcsid_sql_h[] = "$Id: tdsodbc.h,v 1.68 2004-02-11 16:13:17 freddy77 Exp $";
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
	SQLRETURN lastrc;
	int num_errors;
	struct _sql_error *errs;
};

#if ENABLE_EXTRA_CHECKS
void odbc_check_struct_extra(void *p);

#define ODBC_RETURN(handle, rc) \
	do { odbc_check_struct_extra(handle); return (handle->errs.lastrc = (rc)); } while(0)
#else
#define ODBC_RETURN(handle, rc) \
	do { return (handle->errs.lastrc = (rc)); } while(0)
#endif

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
	DSTR sql_desc_base_column_name;
	DSTR sql_desc_base_table_name;
	SQLINTEGER sql_desc_case_sensitive;
	DSTR sql_desc_catalog_name;
	SQLSMALLINT sql_desc_concise_type;
	SQLPOINTER sql_desc_data_ptr;
	SQLSMALLINT sql_desc_datetime_interval_code;
	SQLINTEGER sql_desc_datetime_interval_precision;
	SQLINTEGER sql_desc_display_size;
	SQLSMALLINT sql_desc_fixed_prec_scale;
	SQLINTEGER *sql_desc_indicator_ptr;
	DSTR sql_desc_label;
	SQLUINTEGER sql_desc_length;
	/* this point to a constant buffer, do not free or modify */
	const char *sql_desc_literal_prefix;
	/* this point to a constant buffer, do not free or modify */
	const char *sql_desc_literal_suffix;
	DSTR sql_desc_local_type_name;
	DSTR sql_desc_name;
	SQLSMALLINT sql_desc_nullable;
	SQLINTEGER sql_desc_num_prec_radix;
	SQLINTEGER sql_desc_octet_length;
	SQLINTEGER *sql_desc_octet_length_ptr;
	SQLSMALLINT sql_desc_parameter_type;
	SQLSMALLINT sql_desc_precision;
	SQLSMALLINT sql_desc_rowver;
	SQLSMALLINT sql_desc_scale;
	DSTR sql_desc_schema_name;
	SQLSMALLINT sql_desc_searchable;
	DSTR sql_desc_table_name;
	SQLSMALLINT sql_desc_type;
	/* this point to a constant buffer, do not free or modify */
	const char *sql_desc_type_name;
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
};

typedef struct _hdesc TDS_DESC;

#define DESC_IRD	1
#define DESC_IPD	2
#define DESC_ARD	3
#define DESC_APD	4

struct _heattr
{
	SQLUINTEGER connection_pooling;
	SQLUINTEGER cp_match;
	SQLINTEGER odbc_version;
	SQLINTEGER output_nts;
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
};

struct _hcattr
{
	SQLUINTEGER access_mode;
	SQLUINTEGER async_enable;
	SQLUINTEGER auto_ipd;
	SQLUINTEGER autocommit;
	SQLUINTEGER connection_dead;
	SQLUINTEGER connection_timeout;
	DSTR current_catalog;
	SQLUINTEGER login_timeout;
	SQLUINTEGER metadata_id;
	SQLUINTEGER odbc_cursors;
	SQLUINTEGER packet_size;
	SQLHWND quite_mode;
	DSTR translate_lib;
	SQLUINTEGER translate_option;
	SQLUINTEGER txn_isolation;
#ifdef TDS_NO_DM
	SQLUINTEGER trace;
	DSTR tracefile;
#endif
};

#define TDS_MAX_APP_DESC	100

struct _hstmt;
struct _hdbc
{
	SQLSMALLINT htype;	/* do not reorder this field */
	struct _henv *env;
	TDSSOCKET *tds_socket;
	DSTR dsn;
	DSTR server;		/* aka Instance */
	/** statement executing */
	struct _hstmt *current_statement;
	struct _sql_errors errs;
	struct _hcattr attr;
	TDS_DESC *uad[TDS_MAX_APP_DESC];
};

struct _hsattr
{
	/* TODO remove IRD, ARD, IPD, APD from statement, do not duplicate */
/*	TDS_DESC *app_row_desc; */
/*	TDS_DESC *app_param_desc; */
	SQLUINTEGER async_enable;
	SQLUINTEGER concurrency;
	SQLUINTEGER cursor_scrollable;
	SQLUINTEGER cursor_sensitivity;
	SQLUINTEGER cursor_type;
	SQLUINTEGER enable_auto_ipd;
	SQLPOINTER fetch_bookmark_ptr;
	SQLUINTEGER keyset_size;
	SQLUINTEGER max_length;
	SQLUINTEGER max_rows;
	SQLUINTEGER metadata_id;
	SQLUINTEGER noscan;
	/* apd->sql_desc_bind_offset_ptr */
	/* SQLUINTEGER *param_bind_offset_ptr; */
	/* apd->sql_desc_bind_type */
	/* SQLUINTEGER param_bind_type; */
	/* apd->sql_desc_array_status_ptr */
	/* SQLUSMALLINT *param_operation_ptr; */
	/* ipd->sql_desc_array_status_ptr */
	/* SQLUSMALLINT *param_status_ptr; */
	/* ipd->sql_desc_rows_processed_ptr */
	/* SQLUSMALLINT *params_processed_ptr; */
	/* apd->sql_desc_array_size */
	/* SQLUINTEGER paramset_size; */
	SQLUINTEGER query_timeout;
	SQLUINTEGER retrieve_data;
	/* ard->sql_desc_bind_offset_ptr */
	/* SQLUINTEGER *row_bind_offset_ptr; */
	/* ard->sql_desc_array_size */
	/* SQLUINTEGER row_array_size; */
	/* ard->sql_desc_bind_type */
	/* SQLUINTEGER row_bind_type; */
	SQLUINTEGER row_number;
	/* ard->sql_desc_array_status_ptr */
	/* SQLUINTEGER *row_operation_ptr; */
	/* ird->sql_desc_array_status_ptr */
	/* SQLUINTEGER *row_status_ptr; */
	/* ird->sql_desc_rows_processed_ptr */
	/* SQLUINTEGER *rows_fetched_ptr; */
	SQLUINTEGER simulate_cursor;
	SQLUINTEGER use_bookmarks;
	/* SQLGetStmtAttr only */
/*	TDS_DESC *imp_row_desc; */
/*	TDS_DESC *imp_param_desc; */
};

struct _hstmt
{
	SQLSMALLINT htype;	/* do not reorder this field */
	struct _hdbc *dbc;
	char *query;
	/* begin prepared query stuff */
	char *prepared_query;
	unsigned prepared_query_is_func:1;
	unsigned prepared_query_is_rpc:1;
	/* end prepared query stuff */

	/** parameters saved */
	TDSPARAMINFO *params;
	/** last valid parameter in params, it's a ODBC index (from 1 relative to descriptor) */
	int param_num;

	/** number of parameter in current query */
	unsigned int param_count;
	int row;
	/* do NOT free dynamic, free from socket or attach to connection */
	TDSDYNAMIC *dyn;
	struct _sql_errors errs;
	TDS_DESC *ard, *ird, *apd, *ipd;
	TDS_DESC *orig_ard, *orig_apd;
	SQLUINTEGER sql_rowset_size;
	struct _hsattr attr;
	DSTR cursor_name;	/* auto generated cursor name */
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
SQLRETURN desc_copy(TDS_DESC * dest, TDS_DESC * src);
SQLRETURN desc_free_records(TDS_DESC * desc);

/* fix a bug in MingW headers */
#ifdef __MINGW32__
#if SQL_INTERVAL_YEAR == (100 + SQL_CODE_SECOND)

#undef SQL_INTERVAL_YEAR
#undef SQL_INTERVAL_MONTH
#undef SQL_INTERVAL_DAY
#undef SQL_INTERVAL_HOUR
#undef SQL_INTERVAL_MINUTE
#undef SQL_INTERVAL_SECOND
#undef SQL_INTERVAL_YEAR_TO_MONTH
#undef SQL_INTERVAL_DAY_TO_HOUR
#undef SQL_INTERVAL_DAY_TO_MINUTE
#undef SQL_INTERVAL_DAY_TO_SECOND
#undef SQL_INTERVAL_HOUR_TO_MINUTE
#undef SQL_INTERVAL_HOUR_TO_SECOND
#undef SQL_INTERVAL_MINUTE_TO_SECOND

#define SQL_INTERVAL_YEAR					(100 + SQL_CODE_YEAR)
#define SQL_INTERVAL_MONTH					(100 + SQL_CODE_MONTH)
#define SQL_INTERVAL_DAY					(100 + SQL_CODE_DAY)
#define SQL_INTERVAL_HOUR					(100 + SQL_CODE_HOUR)
#define SQL_INTERVAL_MINUTE					(100 + SQL_CODE_MINUTE)
#define SQL_INTERVAL_SECOND                	(100 + SQL_CODE_SECOND)
#define SQL_INTERVAL_YEAR_TO_MONTH			(100 + SQL_CODE_YEAR_TO_MONTH)
#define SQL_INTERVAL_DAY_TO_HOUR			(100 + SQL_CODE_DAY_TO_HOUR)
#define SQL_INTERVAL_DAY_TO_MINUTE			(100 + SQL_CODE_DAY_TO_MINUTE)
#define SQL_INTERVAL_DAY_TO_SECOND			(100 + SQL_CODE_DAY_TO_SECOND)
#define SQL_INTERVAL_HOUR_TO_MINUTE			(100 + SQL_CODE_HOUR_TO_MINUTE)
#define SQL_INTERVAL_HOUR_TO_SECOND			(100 + SQL_CODE_HOUR_TO_SECOND)
#define SQL_INTERVAL_MINUTE_TO_SECOND		(100 + SQL_CODE_MINUTE_TO_SECOND)

#endif
#endif

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
