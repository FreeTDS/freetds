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

/***************************************************************
 * PROGRAMMER   NAME            CONTACT
 *==============================================================
 * BSB          Brian Bruns     camber@ais.org
 * PAH          Peter Harvey    pharvey@codebydesign.com
 *
 ***************************************************************
 * DATE         PROGRAMMER  CHANGE
 *==============================================================
 *                          Original.
 * 03.FEB.02    PAH         Started adding use of SQLGetPrivateProfileString().
 * 04.FEB.02	PAH         Fixed small error preventing SQLBindParameter from being called
 *
 ***************************************************************/

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "tdsconvert.h"

#include "connectparams.h"
#include "odbc_util.h"
#include "convert_tds2sql.h"
#include "sql2tds.h"
#include "prepare_query.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: odbc.c,v 1.168 2003-05-19 09:25:04 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR * phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR * phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR * phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);
static int mymessagehandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg);
static int myerrorhandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg);
static void log_unimplemented_type(const char function_name[], int fType);
static SQLRETURN SQL_API _SQLExecute(TDS_STMT * stmt);
static void odbc_upper_column_names(TDS_STMT * stmt);
static int odbc_col_setname(TDS_STMT * stmt, int colpos, char *name);
static SQLRETURN odbc_stat_execute(TDS_STMT * stmt, const char *begin, int nparams, ...);


/**
 * \defgroup odbc_api ODBC API
 * Functions callable by \c ODBC client programs
 */


/* utils to check handles */
#define CHECK_HDBC  if ( SQL_NULL_HDBC  == hdbc  ) return SQL_INVALID_HANDLE;
#define CHECK_HSTMT if ( SQL_NULL_HSTMT == hstmt ) return SQL_INVALID_HANDLE;
#define CHECK_HENV  if ( SQL_NULL_HENV  == henv  ) return SQL_INVALID_HANDLE;

#define INIT_HSTMT \
	TDS_STMT *stmt = (TDS_STMT*)hstmt; \
	CHECK_HSTMT; \
	odbc_errs_reset(&stmt->errs); \

#define INIT_HDBC \
	TDS_DBC *dbc = (TDS_DBC*)hdbc; \
	CHECK_HDBC; \
	odbc_errs_reset(&dbc->errs); \

#define INIT_HENV \
	TDS_ENV *env = (TDS_ENV*)henv; \
	CHECK_HENV; \
	odbc_errs_reset(&env->errs); \


/*
**
** Note: I *HATE* hungarian notation, it has to be the most idiotic thing
** I've ever seen. So, you will note it is avoided other than in the function
** declarations. "Gee, let's make our code totally hard to read and they'll
** beg for GUI tools"
** Bah!
*/

static int
odbc_col_setname(TDS_STMT * stmt, int colpos, char *name)
{
	TDSRESULTINFO *resinfo;
	int retcode = -1;

	if (colpos > 0 && stmt->hdbc->tds_socket != NULL && (resinfo = stmt->hdbc->tds_socket->res_info) != NULL) {
		if (colpos <= resinfo->num_cols) {
			/* TODO set column_namelen, see overflow */
			strcpy(resinfo->columns[colpos - 1]->column_name, name);
			retcode = 0;
		}
	}
	return retcode;
}

/* spinellia@acm.org : copied shamelessly from change_database */
static SQLRETURN
change_autocommit(TDS_DBC * dbc, int state)
{
	TDSSOCKET *tds = dbc->tds_socket;
	char query[80];
	TDS_INT result_type;

	/* mssql: SET IMPLICIT_TRANSACTION ON
	 * sybase: SET CHAINED ON */

	/* implicit transactions are on if autocommit is off :-| */
	if (TDS_IS_MSSQL(tds))
		sprintf(query, "set implicit_transactions %s", (state ? "off" : "on"));
	else
		sprintf(query, "set chained %s", (state ? "off" : "on"));

	tdsdump_log(TDS_DBG_INFO1, "change_autocommit: executing %s\n", query);

	if (tds_submit_query(tds, query, NULL) != TDS_SUCCEED) {
		odbc_errs_add(&dbc->errs, ODBCERR_GENERIC, "Could not change transaction status");
		return SQL_ERROR;
	}

	if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
		return SQL_ERROR;

	dbc->autocommit_state = state;
	return SQL_SUCCESS;
}

static void
odbc_env_change(TDSSOCKET * tds, int type, char *oldval, char *newval)
{
	TDS_DBC *dbc;

	if (tds == NULL) {
		return;
	}
	dbc = (TDS_DBC *) tds->parent;
	if (!dbc)
		return;

	switch (type) {
	case TDS_ENV_DATABASE:
		tds_dstr_copy(&dbc->current_database, newval);
		break;
	case TDS_ENV_CHARSET:
		break;
	}
}

static SQLRETURN
do_connect(TDS_DBC * dbc, TDSCONNECTINFO * connect_info)
{
	TDS_ENV *env = dbc->henv;

	dbc->tds_socket = tds_alloc_socket(env->tds_ctx, 512);
	if (!dbc->tds_socket) {
		odbc_errs_add(&dbc->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}
	tds_set_parent(dbc->tds_socket, (void *) dbc);
	dbc->tds_socket->env_chg_func = odbc_env_change;
	tds_fix_connect(connect_info);

	/* fix login type */
	if (!connect_info->try_domain_login) {
		if (strchr(tds_dstr_cstr(&connect_info->user_name), '\\')) {
			connect_info->try_domain_login = 1;
			connect_info->try_server_login = 0;
		}
	}
	if (!connect_info->try_domain_login && !connect_info->try_server_login)
		connect_info->try_server_login = 1;

	if (tds_connect(dbc->tds_socket, connect_info) == TDS_FAIL) {
		dbc->tds_socket = NULL;
		odbc_errs_add(&dbc->errs, ODBCERR_CONNECT, NULL);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLDriverConnect(SQLHDBC hdbc, SQLHWND hwnd, SQLCHAR FAR * szConnStrIn, SQLSMALLINT cbConnStrIn, SQLCHAR FAR * szConnStrOut,
		 SQLSMALLINT cbConnStrOutMax, SQLSMALLINT FAR * pcbConnStrOut, SQLUSMALLINT fDriverCompletion)
{
	SQLRETURN ret;
	TDSCONNECTINFO *connect_info;

	INIT_HDBC;

	connect_info = tds_alloc_connect(dbc->henv->tds_ctx->locale);
	if (!connect_info) {
		odbc_errs_add(&dbc->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}

	/* FIXME szConnStrIn can be no-null terminated */
	tdoParseConnectString((char *) szConnStrIn, connect_info);

	if (tds_dstr_isempty(&connect_info->server_name)) {
		tds_free_connect(connect_info);
		odbc_errs_add(&dbc->errs, ODBCERR_NODSN, "Could not find Servername or server parameter");
		return SQL_ERROR;
	}

	if (tds_dstr_isempty(&connect_info->user_name)) {
		tds_free_connect(connect_info);
		odbc_errs_add(&dbc->errs, ODBCERR_NODSN, "Could not find UID parameter");
		return SQL_ERROR;
	}

	if ((ret = do_connect(dbc, connect_info)) != SQL_SUCCESS) {
		tds_free_connect(connect_info);
		return ret;
	}

	/* use the default database */
	tds_free_connect(connect_info);
	if (dbc->errs.num_errors != 0)
		return SQL_SUCCESS_WITH_INFO;
	return SQL_SUCCESS;
}

#if 0
SQLRETURN SQL_API
SQLBrowseConnect(SQLHDBC hdbc, SQLCHAR FAR * szConnStrIn, SQLSMALLINT cbConnStrIn, SQLCHAR FAR * szConnStrOut,
		 SQLSMALLINT cbConnStrOutMax, SQLSMALLINT FAR * pcbConnStrOut)
{
	INIT_HDBC;
	odbc_errs_add(&dbc->errs, ODBCERR_NOTIMPLEMENTED, "SQLBrowseConnect: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLColumnPrivileges(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		    SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLCHAR FAR * szColumnName,
		    SQLSMALLINT cbColumnName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_column_privileges ", 4,
				  "@table_qualifier", szCatalogName, cbCatalogName,
				  "@table_owner", szSchemaName, cbSchemaName,
				  "@table_name", szTableName, cbTableName, "@column_name", szColumnName, cbColumnName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	return retcode;
}

#if 0
SQLRETURN SQL_API
SQLDescribeParam(SQLHSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT FAR * pfSqlType, SQLUINTEGER FAR * pcbParamDef,
		 SQLSMALLINT FAR * pibScale, SQLSMALLINT FAR * pfNullable)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLDescribeParam: function not implemented");
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLExtendedFetch(SQLHSTMT hstmt, SQLUSMALLINT fFetchType, SQLINTEGER irow, SQLUINTEGER FAR * pcrow, SQLUSMALLINT FAR * rgfRowStatus)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLExtendedFetch: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLForeignKeys(SQLHSTMT hstmt, SQLCHAR FAR * szPkCatalogName, SQLSMALLINT cbPkCatalogName, SQLCHAR FAR * szPkSchemaName,
	       SQLSMALLINT cbPkSchemaName, SQLCHAR FAR * szPkTableName, SQLSMALLINT cbPkTableName, SQLCHAR FAR * szFkCatalogName,
	       SQLSMALLINT cbFkCatalogName, SQLCHAR FAR * szFkSchemaName, SQLSMALLINT cbFkSchemaName, SQLCHAR FAR * szFkTableName,
	       SQLSMALLINT cbFkTableName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_fkeys ", 6,
				  "@pktable_qualifier", szPkCatalogName, cbPkCatalogName,
				  "@pktable_owner", szPkSchemaName, cbPkSchemaName,
				  "@pktable_name", szPkTableName, cbPkTableName,
				  "@fktable_qualifier", szFkCatalogName, cbFkCatalogName,
				  "@fktable_owner", szFkSchemaName, cbFkSchemaName, "@fktable_name", szFkTableName, cbFkTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "PKTABLE_CAT");
		odbc_col_setname(stmt, 2, "PKTABLE_SCHEM");
		odbc_col_setname(stmt, 5, "FKTABLE_CAT");
		odbc_col_setname(stmt, 6, "FKTABLE_SCHEM");
	}
	return retcode;
}

SQLRETURN SQL_API
SQLMoreResults(SQLHSTMT hstmt)
{
	TDSSOCKET *tds;
	TDS_INT result_type;
	int tdsret;
	TDS_INT rowtype;

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	/* try to go to the next recordset */
	for (;;) {
		switch (tds_process_result_tokens(tds, &result_type)) {
		case TDS_NO_MORE_RESULTS:
			if (stmt->hdbc->current_statement == stmt)
				stmt->hdbc->current_statement = NULL;
			return SQL_NO_DATA_FOUND;
		case TDS_SUCCEED:
			switch (result_type) {
			case TDS_COMPUTE_RESULT:
			case TDS_ROW_RESULT:
				/* Skipping current result set's rows to access next resultset or proc's retval */
				while ((tdsret = tds_process_row_tokens(tds, &rowtype, NULL)) == TDS_SUCCEED);
				if (tdsret == TDS_FAIL)
					return SQL_ERROR;
				break;
			case TDS_CMD_FAIL:
				/* FIXME this row is used only as a flag for update binding, should be cleared if binding/result changed */
				stmt->row = 0;
				return SQL_SUCCESS;

			case TDS_STATUS_RESULT:
				odbc_set_return_status(stmt);
				break;

				/* ?? */
			case TDS_CMD_DONE:
				stmt->row = 0;
				break;

			case TDS_COMPUTEFMT_RESULT:
			case TDS_ROWFMT_RESULT:
				stmt->row = 0;
				return SQL_SUCCESS;
			case TDS_PARAM_RESULT:
			case TDS_MSG_RESULT:
			case TDS_DESCRIBE_RESULT:
				break;
			}
		}
	}
	return SQL_ERROR;
}

#if 0
SQLRETURN SQL_API
SQLNativeSql(SQLHDBC hdbc, SQLCHAR FAR * szSqlStrIn, SQLINTEGER cbSqlStrIn, SQLCHAR FAR * szSqlStr, SQLINTEGER cbSqlStrMax,
	     SQLINTEGER FAR * pcbSqlStr)
{
	INIT_HDBC;
	odbc_errs_add(&dbc->errs, ODBCERR_NOTIMPLEMENTED, "SQLNativeSql: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLNumParams(SQLHSTMT hstmt, SQLSMALLINT FAR * pcpar)
{
	INIT_HSTMT;
	*pcpar = stmt->param_count;
	return SQL_SUCCESS;
}

#if 0
SQLRETURN SQL_API
SQLParamOptions(SQLHSTMT hstmt, SQLUINTEGER crow, SQLUINTEGER FAR * pirow)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLParamOptions: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLPrimaryKeys(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	       SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_pkeys ", 3,
				  "@table_qualifier", szCatalogName, cbCatalogName,
				  "@table_owner", szSchemaName, cbSchemaName, "@table_name", szTableName, cbTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	return retcode;
}

SQLRETURN SQL_API
SQLProcedureColumns(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		    SQLSMALLINT cbSchemaName, SQLCHAR FAR * szProcName, SQLSMALLINT cbProcName, SQLCHAR FAR * szColumnName,
		    SQLSMALLINT cbColumnName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_sproc_columns ", 4,
				  "@procedure_qualifier", szCatalogName, cbCatalogName,
				  "@procedure_owner", szSchemaName, cbSchemaName,
				  "@procedure_name", szProcName, cbProcName, "@column_name", szColumnName, cbColumnName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "PROCEDURE_CAT");
		odbc_col_setname(stmt, 2, "PROCEDURE_SCHEM");
		odbc_col_setname(stmt, 8, "COLUMN_SIZE");
		odbc_col_setname(stmt, 9, "BUFFER_LENGTH");
		odbc_col_setname(stmt, 10, "DECIMAL_DIGITS");
		odbc_col_setname(stmt, 11, "NUM_PREC_RADIX");
	}
	return retcode;
}

SQLRETURN SQL_API
SQLProcedures(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	      SQLSMALLINT cbSchemaName, SQLCHAR FAR * szProcName, SQLSMALLINT cbProcName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_stored_procedures ", 3,
				  "@sp_name", szProcName, cbProcName,
				  "@sp_owner", szSchemaName, cbSchemaName, "@sp_qualifier", szCatalogName, cbCatalogName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "PROCEDURE_CAT");
		odbc_col_setname(stmt, 2, "PROCEDURE_SCHEM");
	}
	return retcode;
}

#if 0
SQLRETURN SQL_API
SQLSetPos(SQLHSTMT hstmt, SQLUSMALLINT irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLSetPos: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLTablePrivileges(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		   SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_table_privileges ", 3,
				  "@table_qualifier", szCatalogName, cbCatalogName,
				  "@table_owner", szSchemaName, cbSchemaName, "@table_name", szTableName, cbTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	return retcode;
}

#if (ODBCVER >= 0x0300)
#ifndef SQLULEN
/* unixodbc began defining SQLULEN in recent versions; this lets us complile if you're using an older version. */
# define SQLULEN SQLUINTEGER
#endif
SQLRETURN SQL_API
SQLSetEnvAttr(SQLHENV henv, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER StringLength)
{
	INIT_HENV;
	switch (Attribute) {
	case SQL_ATTR_ODBC_VERSION:
		switch ((SQLULEN) Value) {
		case SQL_OV_ODBC3:
			env->odbc_ver = 3;
			return SQL_SUCCESS;
		case SQL_OV_ODBC2:
			env->odbc_ver = 2;
			return SQL_SUCCESS;
		}
		break;
	}
	odbc_errs_add(&env->errs, ODBCERR_NOTIMPLEMENTED, "SQLSetEnvAttr: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLBindParameter(SQLHSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType,
		 SQLUINTEGER cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER cbValueMax, SQLINTEGER FAR * pcbValue)
{
	struct _sql_param_info *cur, *newitem;

	INIT_HSTMT;

	if (ipar == 0) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDINDEX, NULL);
		return SQL_ERROR;
	}

	/* find available item in list */
	cur = odbc_find_param(stmt, ipar);

	if (!cur) {
		/* didn't find it create a new one */
		newitem = (struct _sql_param_info *)
			malloc(sizeof(struct _sql_param_info));
		if (!newitem) {
			odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
			return SQL_ERROR;
		}
		memset(newitem, 0, sizeof(struct _sql_param_info));
		newitem->param_number = ipar;
		cur = newitem;
		cur->next = stmt->param_head;
		stmt->param_head = cur;
	}

	cur->param_type = fParamType;
	cur->param_bindtype = fCType;
	if (fCType == SQL_C_DEFAULT) {
		cur->param_bindtype = odbc_sql_to_c_type_default(fSqlType);
		if (cur->param_bindtype == 0) {
			odbc_errs_add(&stmt->errs, ODBCERR_INVALIDTYPE, NULL);
			return SQL_ERROR;
		}
	} else {
		cur->param_bindtype = fCType;
	}
	cur->param_sqltype = fSqlType;
	if (cur->param_bindtype == SQL_C_CHAR)
		cur->param_bindlen = cbValueMax;
	cur->param_lenbind = pcbValue;
	cur->varaddr = (char *) rgbValue;

	return SQL_SUCCESS;
}

#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API
SQLAllocHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle, SQLHANDLE * OutputHandle)
{
	switch (HandleType) {
	case SQL_HANDLE_STMT:
		return _SQLAllocStmt(InputHandle, OutputHandle);
		break;
	case SQL_HANDLE_DBC:
		return _SQLAllocConnect(InputHandle, OutputHandle);
		break;
	case SQL_HANDLE_ENV:
		return _SQLAllocEnv(OutputHandle);
		break;
	}
	return SQL_ERROR;
}
#endif

static SQLRETURN SQL_API
_SQLAllocConnect(SQLHENV henv, SQLHDBC FAR * phdbc)
{
	TDS_DBC *dbc;

	INIT_HENV;

	dbc = (TDS_DBC *) malloc(sizeof(TDS_DBC));
	if (!dbc) {
		odbc_errs_add(&env->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}

	/* initialize DBC structure */
	memset(dbc, '\0', sizeof(TDS_DBC));
	tds_dstr_init(&dbc->current_database);
	dbc->henv = env;

	/* spinellia@acm.org
	 * after login is enabled autocommit */
	dbc->autocommit_state = 1;
	*phdbc = (SQLHDBC) dbc;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLAllocConnect(SQLHENV henv, SQLHDBC FAR * phdbc)
{
	odbc_errs_reset(&((TDS_ENV *) henv)->errs);
	return _SQLAllocConnect(henv, phdbc);
}

static SQLRETURN SQL_API
_SQLAllocEnv(SQLHENV FAR * phenv)
{
	TDS_ENV *env;
	TDSCONTEXT *ctx;

	env = (TDS_ENV *) malloc(sizeof(TDS_ENV));
	if (!env)
		return SQL_ERROR;

	memset(env, '\0', sizeof(TDS_ENV));
	env->odbc_ver = 2;
	ctx = tds_alloc_context();
	if (!ctx) {
		free(env);
		return SQL_ERROR;
	}
	env->tds_ctx = ctx;
	tds_ctx_set_parent(ctx, env);
	ctx->msg_handler = mymessagehandler;
	ctx->err_handler = myerrorhandler;

	/* ODBC has its own format */
	if (ctx->locale->date_fmt)
		free(ctx->locale->date_fmt);
	ctx->locale->date_fmt = strdup("%Y-%m-%d %H:%M:%S");

	*phenv = (SQLHENV) env;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLAllocEnv(SQLHENV FAR * phenv)
{
	return _SQLAllocEnv(phenv);
}

static SQLRETURN SQL_API
_SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR * phstmt)
{
	TDS_STMT *stmt;

	INIT_HDBC;

	stmt = (TDS_STMT *) malloc(sizeof(TDS_STMT));
	if (!stmt) {
		odbc_errs_add(&dbc->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}
	memset(stmt, '\0', sizeof(TDS_STMT));
	stmt->hdbc = dbc;
	*phstmt = (SQLHSTMT) stmt;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR * phstmt)
{
	INIT_HDBC;
	return _SQLAllocStmt(hdbc, phstmt);
}

SQLRETURN SQL_API
SQLBindCol(SQLHSTMT hstmt, SQLUSMALLINT icol, SQLSMALLINT fCType, SQLPOINTER rgbValue, SQLINTEGER cbValueMax,
	   SQLINTEGER FAR * pcbValue)
{
	struct _sql_bind_info *cur, *prev = NULL, *newitem;

	INIT_HSTMT;
	if (icol <= 0) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDINDEX, NULL);
		return SQL_ERROR;
	}

	/* find available item in list */
	cur = stmt->bind_head;
	while (cur) {
		if (cur->column_number == icol)
			break;
		prev = cur;
		cur = cur->next;
	}

	if (!cur) {
		/* didn't find it create a new one */
		newitem = (struct _sql_bind_info *) malloc(sizeof(struct _sql_bind_info));
		if (!newitem) {
			odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
			return SQL_ERROR;
		}
		memset(newitem, 0, sizeof(struct _sql_bind_info));
		newitem->column_number = icol;
		/* if there's no head yet */
		if (!stmt->bind_head) {
			stmt->bind_head = newitem;
		} else {
			prev->next = newitem;
		}
		cur = newitem;
	}

	cur->column_bindtype = fCType;
	cur->column_bindlen = cbValueMax;
	cur->column_lenbind = (char *) pcbValue;
	cur->varaddr = (char *) rgbValue;

	/* force rebind */
	stmt->row = 0;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLCancel(SQLHSTMT hstmt)
{
	TDSSOCKET *tds;

	INIT_HSTMT;
	tds = stmt->hdbc->tds_socket;

	/* TODO this can fail... */
	tds_send_cancel(tds);
	tds_process_cancel(tds);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLConnect(SQLHDBC hdbc, SQLCHAR FAR * szDSN, SQLSMALLINT cbDSN, SQLCHAR FAR * szUID, SQLSMALLINT cbUID, SQLCHAR FAR * szAuthStr,
	   SQLSMALLINT cbAuthStr)
{
	const char *DSN;
	SQLRETURN result;
	TDSCONNECTINFO *connect_info;

	INIT_HDBC;

	connect_info = tds_alloc_connect(dbc->henv->tds_ctx->locale);
	if (!connect_info) {
		odbc_errs_add(&dbc->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}

	/* data source name */
	/* FIXME DSN can be no-null terminated */
	if (szDSN || (*szDSN))
		DSN = (char *) szDSN;
	else
		DSN = "DEFAULT";

	if (!odbc_get_dsn_info(DSN, connect_info)) {
		tds_free_connect(connect_info);
		odbc_errs_add(&dbc->errs, ODBCERR_NODSN, "Error getting DSN information");
		return SQL_ERROR;
	}

	/* username/password are never saved to ini file, 
	 * so you do not check in ini file */
	/* user id */
	if (szUID && (*szUID))
		tds_dstr_copyn(&connect_info->user_name, (char *) szUID, odbc_get_string_size(cbUID, szUID));

	/* password */
	if (szAuthStr)
		tds_dstr_copyn(&connect_info->password, (char *) szAuthStr, odbc_get_string_size(cbAuthStr, szAuthStr));

	/* DO IT */
	if ((result = do_connect(dbc, connect_info)) != SQL_SUCCESS) {
		tds_free_connect(connect_info);
		return result;
	}

	tds_free_connect(connect_info);
	if (dbc->errs.num_errors != 0)
		return SQL_SUCCESS_WITH_INFO;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLDescribeCol(SQLHSTMT hstmt, SQLUSMALLINT icol, SQLCHAR FAR * szColName, SQLSMALLINT cbColNameMax, SQLSMALLINT FAR * pcbColName,
	       SQLSMALLINT FAR * pfSqlType, SQLUINTEGER FAR * pcbColDef, SQLSMALLINT FAR * pibScale, SQLSMALLINT FAR * pfNullable)
{
	TDSSOCKET *tds;
	TDSCOLINFO *colinfo;
	int cplen;
	SQLRETURN result = SQL_SUCCESS;

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;
	if (icol <= 0 || tds->res_info == NULL || icol > tds->res_info->num_cols) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDINDEX, "Column out of range");
		return SQL_ERROR;
	}
	/* check name length */
	if (cbColNameMax < 0) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDBUFFERLEN, NULL);
		return SQL_ERROR;
	}
	colinfo = tds->res_info->columns[icol - 1];

	/* cbColNameMax can be 0 (to retrieve name length) */
	if (szColName && cbColNameMax) {
		/* straight copy column name up to cbColNameMax */
		cplen = strlen(colinfo->column_name);
		if (cplen >= cbColNameMax) {
			cplen = cbColNameMax - 1;
			odbc_errs_add(&stmt->errs, ODBCERR_DATATRUNCATION, NULL);
			result = SQL_SUCCESS_WITH_INFO;
		}
		strncpy((char *) szColName, colinfo->column_name, cplen);
		szColName[cplen] = '\0';
	}
	if (pcbColName) {
		/* return column name length (without terminator) 
		 * as specification return full length, not copied length */
		*pcbColName = strlen(colinfo->column_name);
	}
	if (pfSqlType) {
		*pfSqlType = odbc_tds_to_sql_type(colinfo->column_type, colinfo->column_size, stmt->hdbc->henv->odbc_ver);
	}

	if (pcbColDef) {
		if (is_numeric_type(colinfo->column_type)) {
			*pcbColDef = colinfo->column_prec;
		} else {
			*pcbColDef = colinfo->column_size;
		}
	}
	if (pibScale) {
		if (is_numeric_type(colinfo->column_type)) {
			*pibScale = colinfo->column_scale;
		} else {
			*pibScale = 0;
		}
	}
	if (pfNullable) {
		*pfNullable = is_nullable_type(colinfo->column_type) ? 1 : 0;
	}
	return result;
}

SQLRETURN SQL_API
SQLColAttributes(SQLHSTMT hstmt, SQLUSMALLINT icol, SQLUSMALLINT fDescType, SQLPOINTER rgbDesc, SQLSMALLINT cbDescMax,
		 SQLSMALLINT FAR * pcbDesc, SQLINTEGER FAR * pfDesc)
{
	TDSSOCKET *tds;
	TDSCOLINFO *colinfo;
	int cplen, len = 0;
	TDS_DBC *dbc;
	SQLRETURN result = SQL_SUCCESS;

	INIT_HSTMT;

	dbc = stmt->hdbc;
	tds = dbc->tds_socket;

	/* dont check column index for these */
	switch (fDescType) {
	case SQL_COLUMN_COUNT:
		if (!tds->res_info) {
			*pfDesc = 0;
		} else {
			*pfDesc = tds->res_info->num_cols;
		}
		return SQL_SUCCESS;
		break;
	}

	if (!tds->res_info) {
		odbc_errs_add(&stmt->errs, ODBCERR_NORESULT, NULL);
		return SQL_ERROR;
	}

	if (icol == 0 || icol > tds->res_info->num_cols) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDINDEX, "Column out of range");
		return SQL_ERROR;
	}
	colinfo = tds->res_info->columns[icol - 1];

	tdsdump_log(TDS_DBG_INFO1, "odbc:SQLColAttributes: fDescType is %d\n", fDescType);
	switch (fDescType) {
	case SQL_COLUMN_NAME:
	case SQL_COLUMN_LABEL:
		len = strlen(colinfo->column_name);
		cplen = len;
		if (len >= cbDescMax) {
			cplen = cbDescMax - 1;
			odbc_errs_add(&stmt->errs, ODBCERR_DATATRUNCATION, NULL);
			result = SQL_SUCCESS_WITH_INFO;
		}
		tdsdump_log(TDS_DBG_INFO2, "SQLColAttributes: copying %d bytes, len = %d, cbDescMax = %d\n", cplen, len, cbDescMax);
		strncpy((char *) rgbDesc, colinfo->column_name, cplen);
		((char *) rgbDesc)[cplen] = '\0';
		/* return length of full string, not only copied part */
		if (pcbDesc) {
			*pcbDesc = len;
		}
		break;
	case SQL_COLUMN_TYPE:
	case SQL_DESC_TYPE:
		*pfDesc = odbc_tds_to_sql_type(colinfo->column_type, colinfo->column_size, stmt->hdbc->henv->odbc_ver);
		tdsdump_log(TDS_DBG_INFO2, "odbc:SQLColAttributes: colinfo->column_type = %d,"
			    " colinfo->column_size = %d," " *pfDesc = %d\n", colinfo->column_type, colinfo->column_size, *pfDesc);
		break;
	case SQL_COLUMN_PRECISION:	/* this section may be wrong */
		switch (colinfo->column_type) {
		case SYBNUMERIC:
		case SYBDECIMAL:
			*pfDesc = colinfo->column_prec;
			break;
		case SYBCHAR:
		case SYBVARCHAR:
			*pfDesc = colinfo->column_size;
			break;
		case SYBDATETIME:
		case SYBDATETIME4:
		case SYBDATETIMN:
			*pfDesc = 30;
			break;
			/* FIXME what to do with other types ?? */
		default:
			*pfDesc = 0;
			break;
		}
		break;
	case SQL_COLUMN_LENGTH:
		*pfDesc = colinfo->column_size;
		break;
	case SQL_COLUMN_DISPLAY_SIZE:
		switch (odbc_tds_to_sql_type(colinfo->column_type, colinfo->column_size, stmt->hdbc->henv->odbc_ver)) {
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
			*pfDesc = colinfo->column_size;
			break;
		case SQL_BIGINT:
			*pfDesc = 20;
			break;
		case SQL_INTEGER:
			*pfDesc = 11;	/* -1000000000 */
			break;
		case SQL_SMALLINT:
			*pfDesc = 6;	/* -10000 */
			break;
		case SQL_TINYINT:
			*pfDesc = 3;	/* 255 */
			break;
		case SQL_DECIMAL:
		case SQL_NUMERIC:
			*pfDesc = colinfo->column_prec + 2;
			break;
		case SQL_DATE:
			/* FIXME check always yyyy-mm-dd ?? */
			*pfDesc = 19;
			break;
		case SQL_TIME:
			/* FIXME check always hh:mm:ss[.fff] */
			*pfDesc = 19;
			break;
		case SQL_TYPE_TIMESTAMP:
		case SQL_TIMESTAMP:
			*pfDesc = 24;	/* FIXME check, always format 
					 * yyyy-mm-dd hh:mm:ss[.fff] ?? */
			/* spinellia@acm.org: int token.c it is 30 should we comply? */
			break;
		case SQL_FLOAT:
		case SQL_REAL:
		case SQL_DOUBLE:
			*pfDesc = 24;	/* FIXME -- what should the correct size be? */
			break;
		case SQL_GUID:
			*pfDesc = 36;
			break;
		default:
			/* FIXME TODO finish, should support ALL types (interval) */
			*pfDesc = 40;
			tdsdump_log(TDS_DBG_INFO1,
				    "SQLColAttributes(%d,SQL_COLUMN_DISPLAY_SIZE): unknown client type %d\n",
				    icol, odbc_tds_to_sql_type(colinfo->column_type, colinfo->column_size,
							       stmt->hdbc->henv->odbc_ver)
				);
			break;
		}
		break;
		/* FIXME other types ... */
	default:
		tdsdump_log(TDS_DBG_INFO2, "odbc:SQLColAttributes: fDescType %d not catered for...\n");
		break;
	}
	return result;
}


SQLRETURN SQL_API
SQLDisconnect(SQLHDBC hdbc)
{
	INIT_HDBC;

	tds_free_socket(dbc->tds_socket);
	dbc->tds_socket = NULL;

	/* TODO free all associated statements (done by DM??) f77 */

	return SQL_SUCCESS;
}

static int
mymessagehandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg)
{
	struct _sql_errors *errs = NULL;
	TDS_DBC *dbc;

	/*
	 * if (asprintf(&p,
	 * " Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
	 * msg->msg_number, msg->msg_level, msg->msg_state, msg->server, msg->line_number, msg->message) < 0)
	 * return 0;
	 */
	/* latest_msg_number = msg->msg_number; */
	if (tds && tds->parent) {
		dbc = (TDS_DBC *) tds->parent;
		errs = &dbc->errs;
		if (dbc->current_statement)
			errs = &dbc->current_statement->errs;
	} else if (ctx->parent) {
		errs = &((TDS_ENV *) ctx->parent)->errs;
	}
	if (errs)
		odbc_errs_add_rdbms(errs, ODBCERR_GENERIC, msg->message, msg->sql_state, msg->msg_number, msg->line_number,
				    msg->msg_level);
	return 1;
}

static int
myerrorhandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg)
{
	struct _sql_errors *errs = NULL;
	TDS_DBC *dbc;

	/*
	 * if (asprintf(&p,
	 * " Err %d, Level %d, State %d, Server %s, Line %d\n%s\n",
	 * msg->msg_number, msg->msg_level, msg->msg_state, msg->server, msg->line_number, msg->message) < 0)
	 * return 0;
	 */
	if (tds && tds->parent) {
		dbc = (TDS_DBC *) tds->parent;
		errs = &dbc->errs;
		if (dbc->current_statement)
			errs = &dbc->current_statement->errs;
	} else if (ctx->parent) {
		errs = &((TDS_ENV *) ctx->parent)->errs;
	}
	if (errs)
		odbc_errs_add_rdbms(errs, ODBCERR_GENERIC, msg->message, msg->sql_state, msg->msg_number, msg->line_number,
				    msg->msg_level);
	return 1;
}

static SQLRETURN SQL_API
_SQLExecute(TDS_STMT * stmt)
{
	int ret;
	TDSSOCKET *tds = stmt->hdbc->tds_socket;
	TDS_INT result_type;
	TDS_INT done = 0;
	SQLRETURN result = SQL_SUCCESS;

	stmt->row = 0;

	/* TODO submit rpc with more parameters */
	if (stmt->param_count == 0 && stmt->prepared_query_is_rpc) {
		/* get rpc name */
		/* TODO change method */
		char *end = stmt->query, tmp;

		if (*end == '[')
			end = (char *) tds_skip_quoted(end);
		else
			while (!isspace(*++end));
		tmp = *end;
		*end = 0;
		ret = tds_submit_rpc(tds, stmt->query, NULL);
		*end = tmp;
		if (ret != TDS_SUCCEED)
			return SQL_ERROR;
	} else if (!(tds_submit_query(tds, stmt->query, NULL) == TDS_SUCCEED))
		return SQL_ERROR;
	stmt->hdbc->current_statement = stmt;

	/* TODO review this, ODBC return parameter in other way, for compute I don't know */
	while ((ret = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_COMPUTE_RESULT:
		case TDS_PARAM_RESULT:
		case TDS_ROW_RESULT:
			done = 1;
			break;
		case TDS_STATUS_RESULT:
			odbc_set_return_status(stmt);
			break;
		case TDS_CMD_FAIL:
			result = SQL_ERROR;
			done = 1;
			break;

		case TDS_CMD_DONE:
			/* FIXME should just return ?? what happen on INSERT query ? */
			if (tds->res_info)
				done = 1;
			break;
			/* ignore metadata, stop at done or row */
		case TDS_COMPUTEFMT_RESULT:
		case TDS_ROWFMT_RESULT:
			break;
		case TDS_MSG_RESULT:
		case TDS_DESCRIBE_RESULT:
			break;

		}
		if (done)
			break;
	}
	switch (ret) {
	case TDS_NO_MORE_RESULTS:
		if (result == SQL_SUCCESS && stmt->errs.num_errors != 0)
			return SQL_SUCCESS_WITH_INFO;
		return result;
	case TDS_SUCCEED:
		if (result == SQL_SUCCESS && stmt->errs.num_errors != 0)
			return SQL_SUCCESS_WITH_INFO;
		return result;
	default:
		/* TODO test what happened, report correct error to client */
		tdsdump_log(TDS_DBG_INFO1, "SQLExecute: bad results\n");
		return SQL_ERROR;
	}
}

SQLRETURN SQL_API
SQLExecDirect(SQLHSTMT hstmt, SQLCHAR FAR * szSqlStr, SQLINTEGER cbSqlStr)
{
	INIT_HSTMT;

	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, (char *) szSqlStr, cbSqlStr))
		return SQL_ERROR;

	/* count placeholders */
	/* note: szSqlStr can be no-null terminated, so first we set query and then count placeholders */
	stmt->param_count = tds_count_placeholders(stmt->query);

	if (SQL_SUCCESS != prepare_call(stmt))
		return SQL_ERROR;

	if (stmt->param_count) {
		SQLRETURN res;

		assert(stmt->prepared_query == NULL);
		stmt->prepared_query = stmt->query;
		stmt->query = NULL;

		res = start_parse_prepared_query(stmt);

		if (SQL_SUCCESS != res)
			return res;
	}

	return _SQLExecute(stmt);
}

SQLRETURN SQL_API
SQLExecute(SQLHSTMT hstmt)
{
#ifdef ENABLE_DEVELOPING
	TDSSOCKET *tds;
	TDSDYNAMIC *dyn;
	struct _sql_param_info *param;
	TDS_INT result_type;
	int ret, done;
	SQLRETURN result = SQL_SUCCESS;
#endif

	INIT_HSTMT;

#ifdef ENABLE_DEVELOPING
	tds = stmt->hdbc->tds_socket;

	if (stmt->param_count > 0) {
		/* TODO what happen if binding is dynamic (data incomplete?) */

		/* TODO rebuild should be done for every bingings change, not every time */
		int i, nparam;
		TDSPARAMINFO *params = NULL, *temp_params;
		TDSCOLINFO *curcol;

		/* build parameters list */
		tdsdump_log(TDS_DBG_INFO1, "Setting input parameters\n");
		for (i = (stmt->prepared_query_is_func ? 1 : 0), nparam = 0; ++i <= (int) stmt->param_count; ++nparam) {
			/* find binded parameter */
			param = odbc_find_param(stmt, i);
			if (!param) {
				tds_free_param_results(params);
				return SQL_ERROR;
			}

			/* add a columns to parameters */
			if (!(temp_params = tds_alloc_param_result(params))) {
				tds_free_param_results(params);
				odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
				return SQL_ERROR;
			}
			params = temp_params;

			/* add another type and copy data */
			curcol = params->columns[nparam];
			if (sql2tds(stmt->hdbc, param, params, curcol) < 0) {
				tds_free_param_results(params);
				return SQL_ERROR;
			}
		}

		/* TODO test if two SQLPrepare on a statement */
		/* TODO unprepare on statement free of connection close */
		/* prepare dynamic query (only for first SQLExecute call) */
		if (!stmt->dyn && !stmt->prepared_query_is_rpc) {
			TDS_INT result_type;

			tdsdump_log(TDS_DBG_INFO1, "Creating prepared statement\n");
			/* TODO use tds_submit_prepexec */
			if (tds_submit_prepare(tds, stmt->prepared_query, NULL, &stmt->dyn, params) == TDS_FAIL) {
				tds_free_param_results(params);
				return SQL_ERROR;
			}
			if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL) {
				tds_free_param_results(params);
				return SQL_ERROR;
			}
		}
		if (!stmt->prepared_query_is_rpc) {
			dyn = stmt->dyn;
			tds_free_input_params(dyn);
			dyn->params = params;
			tdsdump_log(TDS_DBG_INFO1, "End prepare, execute\n");
			/* TODO return error to client */
			if (tds_submit_execute(tds, dyn) == TDS_FAIL)
				return SQL_ERROR;
		} else {
			/* get rpc name */
			/* TODO change method */
			char *end = stmt->prepared_query, tmp;

			if (*end == '[')
				end = (char *) tds_skip_quoted(end);
			else
				while (!isspace(*++end));
			tmp = *end;
			*end = 0;
			ret = tds_submit_rpc(tds, stmt->prepared_query, params);
			*end = tmp;
			tds_free_param_results(params);
			if (ret != TDS_SUCCEED)
				return SQL_ERROR;
		}

		/* TODO copied from _SQLExecute, use a function... */
		stmt->hdbc->current_statement = stmt;

		done = 0;
		while ((ret = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
			switch (result_type) {
			case TDS_COMPUTE_RESULT:
			case TDS_ROW_RESULT:
				done = 1;
				break;
			case TDS_CMD_FAIL:
				result = SQL_ERROR;
				done = 1;
				break;

			case TDS_CMD_DONE:
				done = 1;
				break;

			case TDS_PARAM_RESULT:
			case TDS_STATUS_RESULT:
			case TDS_COMPUTEFMT_RESULT:
			case TDS_MSG_RESULT:
			case TDS_ROWFMT_RESULT:
			case TDS_DESCRIBE_RESULT:
				break;

			}
			if (done)
				break;
		}
		if (ret == TDS_NO_MORE_RESULTS) {
			odbc_set_return_status(stmt);
			if (result == SQL_ERROR) {
				return SQL_ERROR;
			}
			return SQL_NO_DATA_FOUND;
		}
		return result;
	}
#endif

	if (stmt->prepared_query) {
		SQLRETURN res = start_parse_prepared_query(stmt);

		if (SQL_SUCCESS != res)
			return res;
	}

	return _SQLExecute(stmt);
}

SQLRETURN SQL_API
SQLFetch(SQLHSTMT hstmt)
{
	int ret;
	TDSSOCKET *tds;
	TDSRESULTINFO *resinfo;
	TDSCOLINFO *colinfo;
	int i;
	SQLINTEGER len = 0;
	TDS_CHAR *src;
	int srclen;
	struct _sql_bind_info *cur;
	TDSLOCALE *locale;
	TDSCONTEXT *context;
	TDS_INT rowtype;
	TDS_INT computeid;


	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;

	/* if we bound columns, transfer them to res_info now that we have one */
	if (stmt->row == 0) {
		cur = stmt->bind_head;
		while (cur) {
			if (cur->column_number > 0 && cur->column_number <= tds->res_info->num_cols) {
				colinfo = tds->res_info->columns[cur->column_number - 1];
				colinfo->column_varaddr = cur->varaddr;
				colinfo->column_bindtype = cur->column_bindtype;
				colinfo->column_bindlen = cur->column_bindlen;
				colinfo->column_lenbind = cur->column_lenbind;
			} else {
				/* log error ? */
			}
			cur = cur->next;
		}
	}
	stmt->row++;

	ret = tds_process_row_tokens(stmt->hdbc->tds_socket, &rowtype, &computeid);
	if (ret == TDS_NO_MORE_ROWS) {
		tdsdump_log(TDS_DBG_INFO1, "SQLFetch: NO_DATA_FOUND\n");
		return SQL_NO_DATA_FOUND;
	}
	resinfo = tds->res_info;
	if (!resinfo) {
		tdsdump_log(TDS_DBG_INFO1, "SQLFetch: !resinfo\n");
		return SQL_NO_DATA_FOUND;
	}
	for (i = 0; i < resinfo->num_cols; i++) {
		colinfo = resinfo->columns[i];
		colinfo->column_text_sqlgetdatapos = 0;
		if (colinfo->column_varaddr && !tds_get_null(resinfo->current_row, i)) {
			src = (TDS_CHAR *) & resinfo->current_row[colinfo->column_offset];
			if (is_blob_type(colinfo->column_type))
				src = ((TDSBLOBINFO *) src)->textvalue;
			srclen = colinfo->column_cur_size;
			len = convert_tds2sql(context,
					      tds_get_conversion_type(colinfo->column_type, colinfo->column_size),
					      src,
					      srclen, colinfo->column_bindtype, colinfo->column_varaddr, colinfo->column_bindlen);
			if (len < 0)
				return SQL_ERROR;
		}
		if (colinfo->column_lenbind) {
			if (tds_get_null(resinfo->current_row, i))
				*((SQLINTEGER *) colinfo->column_lenbind) = SQL_NULL_DATA;
			else
				*((SQLINTEGER *) colinfo->column_lenbind) = len;
		}
	}
	if (ret == TDS_SUCCEED) {
		return SQL_SUCCESS;
	} else {
		tdsdump_log(TDS_DBG_INFO1, "SQLFetch: !TDS_SUCCEED (%d)\n", ret);
		return SQL_ERROR;
	}
}


#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API
SQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	tdsdump_log(TDS_DBG_INFO1, "SQLFreeHandle(%d, 0x%x)\n", HandleType, Handle);

	switch (HandleType) {
	case SQL_HANDLE_STMT:
		return _SQLFreeStmt(Handle, SQL_DROP);
		break;
	case SQL_HANDLE_DBC:
		return _SQLFreeConnect(Handle);
		break;
	case SQL_HANDLE_ENV:
		return _SQLFreeEnv(Handle);
		break;
	}
	return SQL_ERROR;
}

static SQLRETURN SQL_API
_SQLFreeConnect(SQLHDBC hdbc)
{
	INIT_HDBC;

	tds_free_socket(dbc->tds_socket);
	odbc_errs_reset(&dbc->errs);
	tds_dstr_free(&dbc->current_database);
	free(dbc);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLFreeConnect(SQLHDBC hdbc)
{
	return _SQLFreeConnect(hdbc);
}
#endif

static SQLRETURN SQL_API
_SQLFreeEnv(SQLHENV henv)
{
	INIT_HENV;

	tds_free_context(env->tds_ctx);
	odbc_errs_reset(&env->errs);
	free(env);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLFreeEnv(SQLHENV henv)
{
	return _SQLFreeEnv(henv);
}

static SQLRETURN SQL_API
_SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption)
{
	TDSSOCKET *tds;

	INIT_HSTMT;

	/* check if option correct */
	if (fOption != SQL_DROP && fOption != SQL_CLOSE && fOption != SQL_UNBIND && fOption != SQL_RESET_PARAMS) {
		tdsdump_log(TDS_DBG_ERROR, "odbc:SQLFreeStmt: Unknown option %d\n", fOption);
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}

	/* if we have bound columns, free the temporary list */
	if (fOption == SQL_DROP || fOption == SQL_UNBIND) {
		struct _sql_bind_info *cur, *tmp;

		if (stmt->bind_head) {
			cur = stmt->bind_head;
			while (cur) {
				tmp = cur->next;
				free(cur);
				cur = tmp;
			}
			stmt->bind_head = NULL;
		}
	}

	/* do the same for bound parameters */
	if (fOption == SQL_DROP || fOption == SQL_RESET_PARAMS) {
		struct _sql_param_info *cur, *tmp;

		if (stmt->param_head) {
			cur = stmt->param_head;
			while (cur) {
				tmp = cur->next;
				free(cur);
				cur = tmp;
			}
			stmt->param_head = NULL;
		}
	}

	/* close statement */
	if (fOption == SQL_DROP || fOption == SQL_CLOSE) {
		tds = stmt->hdbc->tds_socket;
		/* 
		 * FIX ME -- otherwise make sure the current statement is complete
		 */
		/* do not close other running query ! */
		if (tds->state != TDS_IDLE && stmt->hdbc->current_statement == stmt) {
			tds_send_cancel(tds);
			tds_process_cancel(tds);
		}

		/* close prepared statement or add to connection */
		if (stmt->dyn) {
			TDS_INT result_type;

			if (tds_submit_unprepare(tds, stmt->dyn) == TDS_SUCCEED) {
				if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
					return SQL_ERROR;
				tds_free_dynamic(stmt->hdbc->tds_socket, stmt->dyn);
				stmt->dyn = NULL;
			} else {
				/* TODO if fail add to odbc to free later, when we are in idle */
				return SQL_ERROR;
			}
		}
	}

	/* free it */
	if (fOption == SQL_DROP) {
		if (stmt->query)
			free(stmt->query);
		if (stmt->prepared_query)
			free(stmt->prepared_query);
		odbc_errs_reset(&stmt->errs);
		if (stmt->hdbc->current_statement == stmt)
			stmt->hdbc->current_statement = NULL;
		free(stmt);
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption)
{
	INIT_HSTMT;
	return _SQLFreeStmt(hstmt, fOption);
}

#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API
SQLGetStmtAttr(SQLHSTMT hstmt, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER BufferLength, SQLINTEGER * StringLength)
{
	SQLINTEGER tmp_len;
	SQLUINTEGER *ui_val = (SQLUINTEGER *) Value;

	INIT_HSTMT;

	if (!StringLength)
		StringLength = &tmp_len;

	switch (Attribute) {
	case SQL_ATTR_ASYNC_ENABLE:
		*ui_val = SQL_ASYNC_ENABLE_OFF;
		break;

	case SQL_ATTR_CONCURRENCY:
		*ui_val = SQL_CONCUR_READ_ONLY;
		break;

	case SQL_ATTR_CURSOR_SCROLLABLE:
		*ui_val = SQL_NONSCROLLABLE;
		break;

	case SQL_ATTR_CURSOR_SENSITIVITY:
		*ui_val = SQL_UNSPECIFIED;
		break;

	case SQL_ATTR_CURSOR_TYPE:
		*ui_val = SQL_CURSOR_FORWARD_ONLY;
		break;

	case SQL_ATTR_ENABLE_AUTO_IPD:
		*ui_val = SQL_FALSE;
		break;

	case SQL_ATTR_KEYSET_SIZE:
	case SQL_ATTR_MAX_LENGTH:
	case SQL_ATTR_MAX_ROWS:
		*ui_val = 0;
		break;

	case SQL_ATTR_NOSCAN:
		*ui_val = SQL_NOSCAN_OFF;
		break;

	case SQL_ATTR_PARAM_BIND_TYPE:
		*ui_val = SQL_PARAM_BIND_BY_COLUMN;
		break;

	case SQL_ATTR_QUERY_TIMEOUT:
		*ui_val = 0;	/* TODO return timeout in seconds */
		break;

	case SQL_ATTR_RETRIEVE_DATA:
		*ui_val = SQL_RD_ON;
		break;

	case SQL_ATTR_ROW_ARRAY_SIZE:
		*ui_val = 1;
		break;

	case SQL_ATTR_ROW_NUMBER:
		*ui_val = 0;	/* TODO */
		break;

	case SQL_ATTR_USE_BOOKMARKS:
		*ui_val = SQL_UB_OFF;
		break;

		/* This make MS ODBC not crash */
	case SQL_ATTR_APP_ROW_DESC:
		*(SQLPOINTER *) Value = &stmt->ard;
		*StringLength = sizeof(SQL_IS_POINTER);
		break;

	case SQL_ATTR_IMP_ROW_DESC:
		*(SQLPOINTER *) Value = &stmt->ird;
		*StringLength = sizeof(SQL_IS_POINTER);
		break;

	case SQL_ATTR_APP_PARAM_DESC:
		*(SQLPOINTER *) Value = &stmt->apd;
		*StringLength = sizeof(SQL_IS_POINTER);
		break;

	case SQL_ATTR_IMP_PARAM_DESC:
		*(SQLPOINTER *) Value = &stmt->ipd;
		*StringLength = sizeof(SQL_IS_POINTER);
		break;

		/* TODO ?? what to do */
	case SQL_ATTR_FETCH_BOOKMARK_PTR:
	case SQL_ATTR_METADATA_ID:
	case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
	case SQL_ATTR_PARAM_OPERATION_PTR:
	case SQL_ATTR_PARAM_STATUS_PTR:
	case SQL_ATTR_PARAMS_PROCESSED_PTR:
	case SQL_ATTR_PARAMSET_SIZE:
	case SQL_ATTR_ROW_BIND_OFFSET_PTR:
	case SQL_ATTR_ROW_BIND_TYPE:
	case SQL_ATTR_ROW_OPERATION_PTR:
	case SQL_ATTR_ROW_STATUS_PTR:
	case SQL_ATTR_ROWS_FETCHED_PTR:
	case SQL_ATTR_SIMULATE_CURSOR:
	default:
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}
#endif

#if 0
SQLRETURN SQL_API
SQLGetCursorName(SQLHSTMT hstmt, SQLCHAR FAR * szCursor, SQLSMALLINT cbCursorMax, SQLSMALLINT FAR * pcbCursor)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLGetCursorName: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLNumResultCols(SQLHSTMT hstmt, SQLSMALLINT FAR * pccol)
{
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo == NULL) {
		/* 3/15/2001 bsb - DBD::ODBC calls SQLNumResultCols on non-result
		 * ** generating queries such as 'drop table' */
		*pccol = 0;
		return SQL_SUCCESS;
	}

	*pccol = resinfo->num_cols;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLPrepare(SQLHSTMT hstmt, SQLCHAR FAR * szSqlStr, SQLINTEGER cbSqlStr)
{
	INIT_HSTMT;

	if (SQL_SUCCESS != odbc_set_stmt_prepared_query(stmt, (char *) szSqlStr, cbSqlStr))
		return SQL_ERROR;

	/* count parameters */
	stmt->param_count = tds_count_placeholders(stmt->prepared_query);

	/* trasform to native (one time, not for every SQLExecute) */
	if (SQL_SUCCESS != prepare_call(stmt))
		return SQL_ERROR;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLRowCount(SQLHSTMT hstmt, SQLINTEGER FAR * pcrow)
{
	TDSSOCKET *tds;

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	/* test is this is current statement */
	if (stmt->hdbc->current_statement != stmt) {
		return SQL_ERROR;
	}
	*pcrow = -1;
	if (tds->rows_affected == TDS_NO_COUNT) {
		if (tds->res_info != NULL && tds->res_info->row_count != 0)
			*pcrow = tds->res_info->row_count;
	} else {
		*pcrow = tds->rows_affected;
	}
	return SQL_SUCCESS;
}

#if 0
SQLRETURN SQL_API
SQLSetCursorName(SQLHSTMT hstmt, SQLCHAR FAR * szCursor, SQLSMALLINT cbCursor)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLSetCursorName: function not implemented");
	return SQL_ERROR;
}
#endif

/* TODO join all this similar function... */
/* spinellia@acm.org : copied shamelessly from change_database */
/* transaction support */
/* 1 = commit, 0 = rollback */
static SQLRETURN
change_transaction(TDS_DBC * dbc, int state)
{
	TDSSOCKET *tds = dbc->tds_socket;
	TDS_INT result_type;

	tdsdump_log(TDS_DBG_INFO1, "change_transaction(0x%x,%d)\n", dbc, state);

	if (tds_submit_query(tds, state ? "commit" : "rollback", NULL) != TDS_SUCCEED) {
		odbc_errs_add(&dbc->errs, ODBCERR_GENERIC, "Could not perform COMMIT or ROLLBACK");
		return SQL_ERROR;
	}

	if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
		return SQL_ERROR;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLTransact(SQLHENV henv, SQLHDBC hdbc, SQLUSMALLINT fType)
{
	int op = (fType == SQL_COMMIT ? 1 : 0);

	/* I may live without a HENV */
	/*     CHECK_HENV; */
	/* ..but not without a HDBC! */
	INIT_HDBC;

	tdsdump_log(TDS_DBG_INFO1, "SQLTransact(0x%x,0x%x,%d)\n", henv, hdbc, fType);
	return change_transaction(dbc, op);
}

#if ODBCVER >= 0x300
SQLRETURN SQL_API
SQLEndTran(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT completionType)
{
	switch (handleType) {
	case SQL_HANDLE_ENV:
		return SQLTransact(handle, NULL, completionType);
	case SQL_HANDLE_DBC:
		return SQLTransact(NULL, handle, completionType);
	}
	return SQL_ERROR;
}
#endif

/* end of transaction support */

#if 0
SQLRETURN SQL_API
SQLSetParam(SQLHSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLUINTEGER cbParamDef,
	    SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER FAR * pcbValue)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLSetParam: function not implemented");
	return SQL_ERROR;
}
#endif

/************************
 * SQLColumns
 ************************
 *
 * Return column information for a table or view. This is
 * mapped to a call to sp_columns which - lucky for us - returns
 * the exact result set we need.
 *
 * exec sp_columns [ @table_name = ] object 
 *                 [ , [ @table_owner = ] owner ] 
 *                 [ , [ @table_qualifier = ] qualifier ] 
 *                 [ , [ @column_name = ] column ] 
 *                 [ , [ @ODBCVer = ] ODBCVer ] 
 *
 ************************/
SQLRETURN SQL_API
SQLColumns(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName,	/* object_qualifier */
	   SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,	/* object_owner */
	   SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName,	/* object_name */
	   SQLSMALLINT cbTableName, SQLCHAR FAR * szColumnName,	/* column_name */
	   SQLSMALLINT cbColumnName)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_columns ", 4,
				  "@table_name", szTableName, cbTableName,
				  "@table_owner", szSchemaName, cbSchemaName,
				  "@table_qualifier", szCatalogName, cbCatalogName, "@column_name", szColumnName, cbColumnName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
		odbc_col_setname(stmt, 7, "COLUMN_SIZE");
		odbc_col_setname(stmt, 8, "BUFFER_LENGTH");
		odbc_col_setname(stmt, 9, "DECIMAL_DIGITS");
		odbc_col_setname(stmt, 10, "NUM_PREC_RADIX");
	}
	return retcode;
}

SQLRETURN SQL_API
SQLGetConnectOption(SQLHDBC hdbc, SQLUSMALLINT fOption, SQLPOINTER pvParam)
{
	/* TODO implement more options
	 * AUTOCOMMIT required by DBD::ODBC
	 */
	INIT_HDBC;

	switch (fOption) {
	case SQL_AUTOCOMMIT:
		*((SQLUINTEGER *) pvParam) = dbc->autocommit_state;
		return SQL_SUCCESS;
	case SQL_TXN_ISOLATION:
		*((SQLUINTEGER *) pvParam) = SQL_TXN_READ_COMMITTED;
		return SQL_SUCCESS;
	default:
		tdsdump_log(TDS_DBG_INFO1, "odbc:SQLGetConnectOption: Statement option %d not implemented\n", fOption);
		odbc_errs_add(&dbc->errs, ODBCERR_GENERIC, "Statement option not implemented");
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLGetData(SQLHSTMT hstmt, SQLUSMALLINT icol, SQLSMALLINT fCType, SQLPOINTER rgbValue, SQLINTEGER cbValueMax,
	   SQLINTEGER FAR * pcbValue)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	TDS_CHAR *src;
	int srclen;
	TDSLOCALE *locale;
	TDSCONTEXT *context;
	SQLINTEGER dummy_cb;
	int nSybType;

	INIT_HSTMT;

	if (!pcbValue)
		pcbValue = &dummy_cb;

	tds = stmt->hdbc->tds_socket;
	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;
	resinfo = tds->res_info;
	if (icol == 0 || icol > tds->res_info->num_cols) {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDINDEX, "Column out of range");
		return SQL_ERROR;
	}
	colinfo = resinfo->columns[icol - 1];

	if (tds_get_null(resinfo->current_row, icol - 1)) {
		*pcbValue = SQL_NULL_DATA;
	} else {
		src = (TDS_CHAR *) & resinfo->current_row[colinfo->column_offset];
		if (is_blob_type(colinfo->column_type)) {
			if (colinfo->column_text_sqlgetdatapos >= colinfo->column_cur_size)
				return SQL_NO_DATA_FOUND;

			/* FIXME why this became < 0 ??? */
			if (colinfo->column_text_sqlgetdatapos > 0)
				src = ((TDSBLOBINFO *) src)->textvalue + colinfo->column_text_sqlgetdatapos;
			else
				src = ((TDSBLOBINFO *) src)->textvalue;

			srclen = colinfo->column_cur_size - colinfo->column_text_sqlgetdatapos;
		} else {
			srclen = colinfo->column_cur_size;
		}
		nSybType = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
		/* TODO add support for SQL_C_DEFAULT */
		*pcbValue = convert_tds2sql(context, nSybType, src, srclen, fCType, (TDS_CHAR *) rgbValue, cbValueMax);
		if (*pcbValue < 0)
			return SQL_ERROR;

		if (is_blob_type(colinfo->column_type)) {
			/* calc how many bytes was readed */
			int readed = cbValueMax;

			/* char is always terminated... */
			/* FIXME test on destination char ??? */
			if (nSybType == SYBTEXT)
				--readed;
			if (readed > *pcbValue)
				readed = *pcbValue;
			colinfo->column_text_sqlgetdatapos += readed;
			/* not all readed ?? */
			if (colinfo->column_text_sqlgetdatapos < colinfo->column_cur_size)
				return SQL_SUCCESS_WITH_INFO;
		}
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLGetFunctions(SQLHDBC hdbc, SQLUSMALLINT fFunction, SQLUSMALLINT FAR * pfExists)
{
	int i;

	INIT_HDBC;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetFunctions: fFunction is %d\n", fFunction);
	switch (fFunction) {
#if (ODBCVER >= 0x0300)
	case SQL_API_ODBC3_ALL_FUNCTIONS:
		for (i = 0; i < SQL_API_ODBC3_ALL_FUNCTIONS_SIZE; ++i) {
			pfExists[i] = 0;
		}

		/* every api available are contained in a macro 
		 * all these macro begin with API followed by 2 letter
		 * first letter mean pre ODBC 3 (_) or ODBC 3 (3)
		 * second letter mean implemented (X) or unimplemented (_)
		 * You should copy these macro 3 times... not very good
		 * but work. Perhaps best method is build the bit array statically
		 * and then use it but I don't know how to build it...
		 */
#define API_X(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define API__(n)
#define API3X(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define API3_(n)
		API_X(SQL_API_SQLALLOCCONNECT);
		API_X(SQL_API_SQLALLOCENV);
		API3X(SQL_API_SQLALLOCHANDLE);
		API_X(SQL_API_SQLALLOCSTMT);
		API_X(SQL_API_SQLBINDCOL);
		API_X(SQL_API_SQLBINDPARAMETER);
		API__(SQL_API_SQLBROWSECONNECT);
		API3_(SQL_API_SQLBULKOPERATIONS);
		API_X(SQL_API_SQLCANCEL);
		API3_(SQL_API_SQLCLOSECURSOR);
		API3_(SQL_API_SQLCOLATTRIBUTE);
		API_X(SQL_API_SQLCOLATTRIBUTES);
		API_X(SQL_API_SQLCOLUMNPRIVILEGES);
		API_X(SQL_API_SQLCOLUMNS);
		API_X(SQL_API_SQLCONNECT);
		API3_(SQL_API_SQLCOPYDESC);
		API_X(SQL_API_SQLDESCRIBECOL);
		API__(SQL_API_SQLDESCRIBEPARAM);
		API_X(SQL_API_SQLDISCONNECT);
		API_X(SQL_API_SQLDRIVERCONNECT);
		API3X(SQL_API_SQLENDTRAN);
		API_X(SQL_API_SQLERROR);
		API_X(SQL_API_SQLEXECDIRECT);
		API_X(SQL_API_SQLEXECUTE);
		API__(SQL_API_SQLEXTENDEDFETCH);
		API_X(SQL_API_SQLFETCH);
		API3_(SQL_API_SQLFETCHSCROLL);
		API_X(SQL_API_SQLFOREIGNKEYS);
		API_X(SQL_API_SQLFREECONNECT);
		API_X(SQL_API_SQLFREEENV);
		API3X(SQL_API_SQLFREEHANDLE);
		API_X(SQL_API_SQLFREESTMT);
		API3_(SQL_API_SQLGETCONNECTATTR);
		API_X(SQL_API_SQLGETCONNECTOPTION);
		API__(SQL_API_SQLGETCURSORNAME);
		API_X(SQL_API_SQLGETDATA);
		API3_(SQL_API_SQLGETDESCFIELD);
		API3_(SQL_API_SQLGETDESCREC);
		API3X(SQL_API_SQLGETDIAGFIELD);
		API3X(SQL_API_SQLGETDIAGREC);
		API3_(SQL_API_SQLGETENVATTR);
		API_X(SQL_API_SQLGETFUNCTIONS);
		API_X(SQL_API_SQLGETINFO);
		API3X(SQL_API_SQLGETSTMTATTR);
		API_X(SQL_API_SQLGETSTMTOPTION);
		API_X(SQL_API_SQLGETTYPEINFO);
		API_X(SQL_API_SQLMORERESULTS);
		API__(SQL_API_SQLNATIVESQL);
		API_X(SQL_API_SQLNUMPARAMS);
		API_X(SQL_API_SQLNUMRESULTCOLS);
		API_X(SQL_API_SQLPARAMDATA);
		API__(SQL_API_SQLPARAMOPTIONS);
		API_X(SQL_API_SQLPREPARE);
		API_X(SQL_API_SQLPRIMARYKEYS);
		API_X(SQL_API_SQLPROCEDURECOLUMNS);
		API_X(SQL_API_SQLPROCEDURES);
		API_X(SQL_API_SQLPUTDATA);
		API_X(SQL_API_SQLROWCOUNT);
		API3X(SQL_API_SQLSETCONNECTATTR);
		API_X(SQL_API_SQLSETCONNECTOPTION);
		API__(SQL_API_SQLSETCURSORNAME);
		API3_(SQL_API_SQLSETDESCFIELD);
		API3_(SQL_API_SQLSETDESCREC);
		API3X(SQL_API_SQLSETENVATTR);
		API__(SQL_API_SQLSETPARAM);
		API__(SQL_API_SQLSETPOS);
		API__(SQL_API_SQLSETSCROLLOPTIONS);
		API3_(SQL_API_SQLSETSTMTATTR);
		API_X(SQL_API_SQLSETSTMTOPTION);
		API__(SQL_API_SQLSPECIALCOLUMNS);
		API_X(SQL_API_SQLSTATISTICS);
		API_X(SQL_API_SQLTABLEPRIVILEGES);
		API_X(SQL_API_SQLTABLES);
		API_X(SQL_API_SQLTRANSACT);
#undef API_X
#undef API__
#undef API3X
#undef API3_
		return SQL_SUCCESS;
#endif

	case SQL_API_ALL_FUNCTIONS:
		tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetFunctions: " "fFunction is SQL_API_ALL_FUNCTIONS\n");
		for (i = 0; i < 100; ++i) {
			pfExists[i] = SQL_FALSE;
		}

#define API_X(n) if (n >= 0 && n < 100) pfExists[n] = SQL_TRUE;
#define API__(n)
#define API3X(n)
#define API3_(n)
		API_X(SQL_API_SQLALLOCCONNECT);
		API_X(SQL_API_SQLALLOCENV);
		API3X(SQL_API_SQLALLOCHANDLE);
		API_X(SQL_API_SQLALLOCSTMT);
		API_X(SQL_API_SQLBINDCOL);
		API_X(SQL_API_SQLBINDPARAMETER);
		API__(SQL_API_SQLBROWSECONNECT);
		API3_(SQL_API_SQLBULKOPERATIONS);
		API_X(SQL_API_SQLCANCEL);
		API3_(SQL_API_SQLCLOSECURSOR);
		API3_(SQL_API_SQLCOLATTRIBUTE);
		API_X(SQL_API_SQLCOLATTRIBUTES);
		API_X(SQL_API_SQLCOLUMNPRIVILEGES);
		API_X(SQL_API_SQLCOLUMNS);
		API_X(SQL_API_SQLCONNECT);
		API3_(SQL_API_SQLCOPYDESC);
		API_X(SQL_API_SQLDESCRIBECOL);
		API__(SQL_API_SQLDESCRIBEPARAM);
		API_X(SQL_API_SQLDISCONNECT);
		API_X(SQL_API_SQLDRIVERCONNECT);
		API3X(SQL_API_SQLENDTRAN);
		API_X(SQL_API_SQLERROR);
		API_X(SQL_API_SQLEXECDIRECT);
		API_X(SQL_API_SQLEXECUTE);
		API__(SQL_API_SQLEXTENDEDFETCH);
		API_X(SQL_API_SQLFETCH);
		API3_(SQL_API_SQLFETCHSCROLL);
		API_X(SQL_API_SQLFOREIGNKEYS);
		API_X(SQL_API_SQLFREECONNECT);
		API_X(SQL_API_SQLFREEENV);
		API3X(SQL_API_SQLFREEHANDLE);
		API_X(SQL_API_SQLFREESTMT);
		API3_(SQL_API_SQLGETCONNECTATTR);
		API_X(SQL_API_SQLGETCONNECTOPTION);
		API__(SQL_API_SQLGETCURSORNAME);
		API_X(SQL_API_SQLGETDATA);
		API3_(SQL_API_SQLGETDESCFIELD);
		API3_(SQL_API_SQLGETDESCREC);
		API3X(SQL_API_SQLGETDIAGFIELD);
		API3X(SQL_API_SQLGETDIAGREC);
		API3_(SQL_API_SQLGETENVATTR);
		API_X(SQL_API_SQLGETFUNCTIONS);
		API_X(SQL_API_SQLGETINFO);
		API3X(SQL_API_SQLGETSTMTATTR);
		API_X(SQL_API_SQLGETSTMTOPTION);
		API_X(SQL_API_SQLGETTYPEINFO);
		API_X(SQL_API_SQLMORERESULTS);
		API__(SQL_API_SQLNATIVESQL);
		API_X(SQL_API_SQLNUMPARAMS);
		API_X(SQL_API_SQLNUMRESULTCOLS);
		API_X(SQL_API_SQLPARAMDATA);
		API__(SQL_API_SQLPARAMOPTIONS);
		API_X(SQL_API_SQLPREPARE);
		API_X(SQL_API_SQLPRIMARYKEYS);
		API_X(SQL_API_SQLPROCEDURECOLUMNS);
		API_X(SQL_API_SQLPROCEDURES);
		API_X(SQL_API_SQLPUTDATA);
		API_X(SQL_API_SQLROWCOUNT);
		API3X(SQL_API_SQLSETCONNECTATTR);
		API_X(SQL_API_SQLSETCONNECTOPTION);
		API__(SQL_API_SQLSETCURSORNAME);
		API3_(SQL_API_SQLSETDESCFIELD);
		API3_(SQL_API_SQLSETDESCREC);
		API3X(SQL_API_SQLSETENVATTR);
		API__(SQL_API_SQLSETPARAM);
		API__(SQL_API_SQLSETPOS);
		API__(SQL_API_SQLSETSCROLLOPTIONS);
		API3_(SQL_API_SQLSETSTMTATTR);
		API_X(SQL_API_SQLSETSTMTOPTION);
		API__(SQL_API_SQLSPECIALCOLUMNS);
		API_X(SQL_API_SQLSTATISTICS);
		API_X(SQL_API_SQLTABLEPRIVILEGES);
		API_X(SQL_API_SQLTABLES);
		API_X(SQL_API_SQLTRANSACT);
#undef API_X
#undef API__
#undef API3X
#undef API3_
		return SQL_SUCCESS;
		break;
#define API_X(n) case n:
#define API__(n)
#if (ODBCVER >= 0x300)
#define API3X(n) case n:
#else
#define API3X(n)
#endif
#define API3_(n)
		API_X(SQL_API_SQLALLOCCONNECT);
		API_X(SQL_API_SQLALLOCENV);
		API3X(SQL_API_SQLALLOCHANDLE);
		API_X(SQL_API_SQLALLOCSTMT);
		API_X(SQL_API_SQLBINDCOL);
		API_X(SQL_API_SQLBINDPARAMETER);
		API__(SQL_API_SQLBROWSECONNECT);
		API3_(SQL_API_SQLBULKOPERATIONS);
		API_X(SQL_API_SQLCANCEL);
		API3_(SQL_API_SQLCLOSECURSOR);
		API3_(SQL_API_SQLCOLATTRIBUTE);
		API_X(SQL_API_SQLCOLATTRIBUTES);
		API_X(SQL_API_SQLCOLUMNPRIVILEGES);
		API_X(SQL_API_SQLCOLUMNS);
		API_X(SQL_API_SQLCONNECT);
		API3_(SQL_API_SQLCOPYDESC);
		API_X(SQL_API_SQLDESCRIBECOL);
		API__(SQL_API_SQLDESCRIBEPARAM);
		API_X(SQL_API_SQLDISCONNECT);
		API_X(SQL_API_SQLDRIVERCONNECT);
		API3X(SQL_API_SQLENDTRAN);
		API_X(SQL_API_SQLERROR);
		API_X(SQL_API_SQLEXECDIRECT);
		API_X(SQL_API_SQLEXECUTE);
		API__(SQL_API_SQLEXTENDEDFETCH);
		API_X(SQL_API_SQLFETCH);
		API3_(SQL_API_SQLFETCHSCROLL);
		API_X(SQL_API_SQLFOREIGNKEYS);
		API_X(SQL_API_SQLFREECONNECT);
		API_X(SQL_API_SQLFREEENV);
		API3X(SQL_API_SQLFREEHANDLE);
		API_X(SQL_API_SQLFREESTMT);
		API3_(SQL_API_SQLGETCONNECTATTR);
		API_X(SQL_API_SQLGETCONNECTOPTION);
		API__(SQL_API_SQLGETCURSORNAME);
		API_X(SQL_API_SQLGETDATA);
		API3_(SQL_API_SQLGETDESCFIELD);
		API3_(SQL_API_SQLGETDESCREC);
		API3X(SQL_API_SQLGETDIAGFIELD);
		API3X(SQL_API_SQLGETDIAGREC);
		API3_(SQL_API_SQLGETENVATTR);
		API_X(SQL_API_SQLGETFUNCTIONS);
		API_X(SQL_API_SQLGETINFO);
		API3X(SQL_API_SQLGETSTMTATTR);
		API_X(SQL_API_SQLGETSTMTOPTION);
		API_X(SQL_API_SQLGETTYPEINFO);
		API_X(SQL_API_SQLMORERESULTS);
		API__(SQL_API_SQLNATIVESQL);
		API_X(SQL_API_SQLNUMPARAMS);
		API_X(SQL_API_SQLNUMRESULTCOLS);
		API_X(SQL_API_SQLPARAMDATA);
		API__(SQL_API_SQLPARAMOPTIONS);
		API_X(SQL_API_SQLPREPARE);
		API_X(SQL_API_SQLPRIMARYKEYS);
		API_X(SQL_API_SQLPROCEDURECOLUMNS);
		API_X(SQL_API_SQLPROCEDURES);
		API_X(SQL_API_SQLPUTDATA);
		API_X(SQL_API_SQLROWCOUNT);
		API3X(SQL_API_SQLSETCONNECTATTR);
		API_X(SQL_API_SQLSETCONNECTOPTION);
		API__(SQL_API_SQLSETCURSORNAME);
		API3_(SQL_API_SQLSETDESCFIELD);
		API3_(SQL_API_SQLSETDESCREC);
		API3X(SQL_API_SQLSETENVATTR);
		API__(SQL_API_SQLSETPARAM);
		API__(SQL_API_SQLSETPOS);
		API__(SQL_API_SQLSETSCROLLOPTIONS);
		API3_(SQL_API_SQLSETSTMTATTR);
		API_X(SQL_API_SQLSETSTMTOPTION);
		API__(SQL_API_SQLSPECIALCOLUMNS);
		API_X(SQL_API_SQLSTATISTICS);
		API_X(SQL_API_SQLTABLEPRIVILEGES);
		API_X(SQL_API_SQLTABLES);
		API_X(SQL_API_SQLTRANSACT);
#undef API_X
#undef API__
#undef API3X
#undef API3_
		*pfExists = SQL_TRUE;
		return SQL_SUCCESS;
	default:
		*pfExists = SQL_FALSE;
		return SQL_SUCCESS;
		break;
	}
	return SQL_SUCCESS;
}


SQLRETURN SQL_API
SQLGetInfo(SQLHDBC hdbc, SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue, SQLSMALLINT cbInfoValueMax,
	   SQLSMALLINT FAR * pcbInfoValue)
{
	const char *p = NULL;
	SQLSMALLINT *siInfoValue = (SQLSMALLINT *) rgbInfoValue;
	SQLUSMALLINT *usiInfoValue = (SQLUSMALLINT *) rgbInfoValue;
	SQLUINTEGER *uiInfoValue = (SQLUINTEGER *) rgbInfoValue;

	INIT_HDBC;

	switch (fInfoType) {
	case SQL_ACTIVE_STATEMENTS:
		*siInfoValue = 1;
		break;
	case SQL_ALTER_TABLE:
		*uiInfoValue = SQL_AT_ADD_COLUMN | SQL_AT_ADD_COLUMN_DEFAULT
			| SQL_AT_ADD_COLUMN_SINGLE | SQL_AT_ADD_CONSTRAINT
			| SQL_AT_ADD_TABLE_CONSTRAINT | SQL_AT_CONSTRAINT_NAME_DEFINITION | SQL_AT_DROP_COLUMN_RESTRICT;
		break;
	case SQL_CATALOG_USAGE:
		*uiInfoValue = SQL_CU_DML_STATEMENTS | SQL_CU_PROCEDURE_INVOCATION | SQL_CU_TABLE_DEFINITION;
		break;
	case SQL_CURSOR_COMMIT_BEHAVIOR:
		/* currently cursors are not supported however sql server close automaticly cursors on commit */
		*usiInfoValue = SQL_CB_CLOSE;
		break;
	case SQL_DATABASE_NAME:
		p = tds_dstr_cstr(&dbc->current_database);
		break;
	case SQL_DATA_SOURCE_READ_ONLY:
		/* TODO: determine the right answer from connection 
		 * attribute SQL_ATTR_ACCESS_MODE */
		*uiInfoValue = 0;	/* false, writable */
		break;
	case SQL_DBMS_NAME:
		/* TODO dbms name and version can be safed from login... */
		if (dbc->tds_socket && TDS_IS_MSSQL(dbc->tds_socket))
			p = "Microsoft SQL Server";
		else
			p = "SQL Server";
		break;
	case SQL_DBMS_VER:
		if (rgbInfoValue && cbInfoValueMax > 5)
			tds_version(dbc->tds_socket, (char *) rgbInfoValue);
		else
			p = "unknown version";
		break;
	case SQL_DEFAULT_TXN_ISOLATION:
		*uiInfoValue = SQL_TXN_READ_COMMITTED;
		break;
	case SQL_DRIVER_VER:
		p = VERSION;
		break;
	case SQL_DRIVER_NAME:	/* ODBC 2.0 */
		p = "libtdsodbc.so";
		break;
	case SQL_DRIVER_ODBC_VER:
		p = "03.00";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
		/* Cursors not supported yet */
		*uiInfoValue = 0;
		break;
#endif
	case SQL_FILE_USAGE:
		*uiInfoValue = SQL_FILE_NOT_SUPPORTED;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
		/* Cursors not supported yet */
		*uiInfoValue = 0;
		break;
#endif
	case SQL_IDENTIFIER_QUOTE_CHAR:
		p = "\"";
		break;
	case SQL_KEYSET_CURSOR_ATTRIBUTES1:
	case SQL_KEYSET_CURSOR_ATTRIBUTES2:
		/* Cursors not supported yet */
		*uiInfoValue = 0;
		break;
	case SQL_NEED_LONG_DATA_LEN:
		/* current implementation do not require length, however future will, so is correct to return yes */
		p = "Y";
		break;
	case SQL_QUOTED_IDENTIFIER_CASE:
		/* TODO usually insensitive */
		*usiInfoValue = SQL_IC_MIXED;
		break;
	case SQL_SCHEMA_USAGE:
		*uiInfoValue =
			SQL_OU_DML_STATEMENTS | SQL_OU_INDEX_DEFINITION | SQL_OU_PRIVILEGE_DEFINITION | SQL_OU_PROCEDURE_INVOCATION
			| SQL_OU_TABLE_DEFINITION;
		break;
	case SQL_SCROLL_OPTIONS:
		*uiInfoValue = SQL_SO_FORWARD_ONLY | SQL_SO_STATIC;
		break;
	case SQL_SCROLL_CONCURRENCY:
		*uiInfoValue = SQL_SCCO_READ_ONLY;
		break;
	case SQL_SPECIAL_CHARACTERS:
		/* TODO others ?? */
		p = "\'\"[]{}";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_STATIC_CURSOR_ATTRIBUTES1:
	case SQL_STATIC_CURSOR_ATTRIBUTES2:
		/* Cursors not supported yet */
		*uiInfoValue = 0;
		break;
#endif
	case SQL_TXN_CAPABLE:
		/* transaction for DML and DDL */
		*siInfoValue = SQL_TC_ALL;
		break;
	case SQL_XOPEN_CLI_YEAR:
		/* TODO check specifications */
		p = "1995";
		break;
		/* TODO support for other options */
	default:
		log_unimplemented_type("SQLGetInfo", fInfoType);
		odbc_errs_add(&dbc->errs, ODBCERR_NOTIMPLEMENTED, "Option not supported");
		return SQL_ERROR;
	}

	/* char data */
	if (p)
		return odbc_set_string(rgbInfoValue, cbInfoValueMax, pcbInfoValue, p, -1);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLGetStmtOption(SQLHSTMT hstmt, SQLUSMALLINT fOption, SQLPOINTER pvParam)
{
	SQLUINTEGER *piParam = (SQLUINTEGER *) pvParam;

	INIT_HSTMT;

	switch (fOption) {
	case SQL_ROWSET_SIZE:
		*piParam = 1;
		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "odbc:SQLGetStmtOption: Statement option %d not implemented\n", fOption);
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}

	return SQL_SUCCESS;
}

static void
odbc_upper_column_names(TDS_STMT * stmt)
{
	TDSRESULTINFO *resinfo;
	TDSCOLINFO *colinfo;
	TDSSOCKET *tds;
	int icol;
	char *p;

	tds = stmt->hdbc->tds_socket;
	if (!tds || !tds->res_info)
		return;

	resinfo = tds->res_info;
	for (icol = 0; icol < resinfo->num_cols; ++icol) {
		colinfo = resinfo->columns[icol];
		/* upper case */
		/* TODO procedure */
		for (p = colinfo->column_name; *p; ++p)
			if ('a' <= *p && *p <= 'z')
				*p = *p & (~0x20);
	}
}

SQLRETURN SQL_API
SQLGetTypeInfo(SQLHSTMT hstmt, SQLSMALLINT fSqlType)
{
	SQLRETURN res;
	TDSSOCKET *tds;
	TDS_INT row_type;
	TDS_INT compute_id;
	int varchar_pos = -1, n;
	static const char sql_templ[] = "EXEC sp_datatype_info %d";
	char sql[sizeof(sql_templ) + 30];

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	/* For MSSQL6.5 and Sybase 11.9 sp_datatype_info work */
	/* FIXME what about early Sybase products ? */
	/* TODO ODBC3 convert type to ODBC version 2 (date) */
	sprintf(sql, sql_templ, fSqlType);
	if (TDS_IS_MSSQL(tds) && stmt->hdbc->henv->odbc_ver == 3)
		strcat(sql, ",3");
	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, sql, strlen(sql)))
		return SQL_ERROR;

      redo:
	res = _SQLExecute(stmt);

	odbc_upper_column_names(stmt);

	if (TDS_IS_MSSQL(stmt->hdbc->tds_socket) || fSqlType != 12 || res != SQL_SUCCESS)
		return res;

	/* Sybase return first nvarchar for char... and without length !!! */
	/* Some program use first entry so we discard all entry bfore varchar */
	n = 0;
	while (tds->res_info) {
		TDSRESULTINFO *resinfo;
		TDSCOLINFO *colinfo;
		char *name;

		/* if next is varchar leave next for SQLFetch */
		if (n == (varchar_pos - 1))
			break;

		switch (tds_process_row_tokens(stmt->hdbc->tds_socket, &row_type, &compute_id)) {
		case TDS_NO_MORE_ROWS:
			while (tds->state == TDS_PENDING)
				tds_process_default_tokens(tds, tds_get_byte(tds));
			if (n >= varchar_pos && varchar_pos > 0)
				goto redo;
			break;
		}
		if (!tds->res_info)
			break;
		++n;

		resinfo = tds->res_info;
		colinfo = resinfo->columns[0];
		name = (char *) (resinfo->current_row + colinfo->column_offset);
		/* skip nvarchar and sysname */
		if (colinfo->column_cur_size == 7 && memcmp("varchar", name, 7) == 0) {
			varchar_pos = n;
		}
	}
	return res;
}

SQLRETURN SQL_API
SQLParamData(SQLHSTMT hstmt, SQLPOINTER FAR * prgbValue)
{
	struct _sql_param_info *param;

	INIT_HSTMT;

	if (stmt->prepared_query_need_bytes) {
		param = odbc_find_param(stmt, stmt->prepared_query_param_num);
		if (!param)
			return SQL_ERROR;

		*prgbValue = param->varaddr;
		return SQL_NEED_DATA;
	}

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLPutData(SQLHSTMT hstmt, SQLPOINTER rgbValue, SQLINTEGER cbValue)
{
	INIT_HSTMT;

	if (stmt->prepared_query && stmt->param_head) {
		SQLRETURN res = continue_parse_prepared_query(stmt, rgbValue, cbValue);

		if (SQL_NEED_DATA == res)
			return SQL_SUCCESS;
		if (SQL_SUCCESS != res)
			return res;
	}

	return _SQLExecute(stmt);
}


SQLRETURN SQL_API
SQLSetConnectAttr(SQLHDBC hdbc, SQLINTEGER Attribute, SQLPOINTER ValuePtr, SQLINTEGER StringLength)
{
	SQLULEN u_value = (SQLULEN) ValuePtr;

	INIT_HDBC;

	switch (Attribute) {
	case SQL_ATTR_AUTOCOMMIT:
		/* spinellia@acm.org */
		if (u_value == SQL_AUTOCOMMIT_ON)
			return change_autocommit(dbc, 1);
		return change_autocommit(dbc, 0);
		break;
/*	case SQL_ATTR_CONNECTION_TIMEOUT:
		dbc->tds_socket->connect_timeout = u_value;
		break; */
	}
	odbc_errs_add(&dbc->errs, ODBCERR_INVALIDOPTION, NULL);
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLSetConnectOption(SQLHDBC hdbc, SQLUSMALLINT fOption, SQLUINTEGER vParam)
{
	INIT_HDBC;

	switch (fOption) {
	case SQL_AUTOCOMMIT:
		/* spinellia@acm.org */
		return SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) vParam, 0);
	default:
		tdsdump_log(TDS_DBG_INFO1, "odbc:SQLSetConnectOption: Statement option %d not implemented\n", fOption);
		odbc_errs_add(&dbc->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLSetStmtOption(SQLHSTMT hstmt, SQLUSMALLINT fOption, SQLUINTEGER vParam)
{
	INIT_HSTMT;

	switch (fOption) {
	case SQL_ROWSET_SIZE:
		/* Always 1 */
		break;
	case SQL_CURSOR_TYPE:
		if (vParam == SQL_CURSOR_FORWARD_ONLY)
			return SQL_SUCCESS;
		/* fall through */
	default:
		tdsdump_log(TDS_DBG_INFO1, "odbc:SQLSetStmtOption: Statement option %d not implemented\n", fOption);
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}

	return SQL_SUCCESS;
}

#if 0
SQLRETURN SQL_API
SQLSpecialColumns(SQLHSTMT hstmt, SQLUSMALLINT fColType, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName,
		  SQLCHAR FAR * szSchemaName, SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName,
		  SQLUSMALLINT fScope, SQLUSMALLINT fNullable)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLSpecialColumns: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLStatistics(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	      SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLUSMALLINT fUnique,
	      SQLUSMALLINT fAccuracy)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_statistics ", 3,
				  "@table_qualifier", szCatalogName, cbCatalogName,
				  "@table_owner", szSchemaName, cbSchemaName, "@table_name", szTableName, cbTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
		odbc_col_setname(stmt, 8, "ORDINAL_POSITION");
		odbc_col_setname(stmt, 10, "ASC_OR_DESC");
	}
	return retcode;
}


SQLRETURN SQL_API
SQLTables(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	  SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLCHAR FAR * szTableType,
	  SQLSMALLINT cbTableType)
{
	int retcode;

	INIT_HSTMT;

	retcode =
		odbc_stat_execute(stmt, "sp_tables ", 4,
				  "@table_name", szTableName, cbTableName,
				  "@table_owner", szSchemaName, cbSchemaName,
				  "@table_qualifier", szCatalogName, cbCatalogName, "@table_type", szTableType, cbTableType);
	if (SQL_SUCCEEDED(retcode) && stmt->hdbc->henv->odbc_ver >= 3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	return retcode;
}

/** 
 * Log a useful message about unimplemented options
 * Defying belief, Microsoft defines mutually exclusive options that
 * some ODBC implementations #define as duplicate values (meaning, of course, 
 * that they couldn't be implemented in the same function because they're 
 * indistinguishable.  
 * 
 * Those duplicates are commented out below.
 */
static void
log_unimplemented_type(const char function_name[], int fType)
{
	const char *name, *category;

	switch (fType) {
	case SQL_ACCESSIBLE_PROCEDURES:
		name = "SQL_ACCESSIBLE_PROCEDURES";
		category = "Data Source Information";
		break;
	case SQL_ACCESSIBLE_TABLES:
		name = "SQL_ACCESSIBLE_TABLES";
		category = "Data Source Information";
		break;
	case SQL_ACTIVE_CONNECTIONS:
		name = "SQL_MAX_DRIVER_CONNECTIONS/SQL_ACTIVE_CONNECTIONS";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_ACTIVE_ENVIRONMENTS:
		name = "SQL_ACTIVE_ENVIRONMENTS";
		category = "Driver Information";
		break;
	case SQL_ACTIVE_STATEMENTS:
		name = "SQL_MAX_CONCURRENT_ACTIVITIES/SQL_ACTIVE_STATEMENTS";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_AGGREGATE_FUNCTIONS:
		name = "SQL_AGGREGATE_FUNCTIONS";
		category = "Supported SQL";
		break;
	case SQL_ALTER_DOMAIN:
		name = "SQL_ALTER_DOMAIN";
		category = "Supported SQL";
		break;
#	ifdef SQL_ALTER_SCHEMA
	case SQL_ALTER_SCHEMA:
		name = "SQL_ALTER_SCHEMA";
		category = "Supported SQL";
		break;
#	endif
	case SQL_ALTER_TABLE:
		name = "SQL_ALTER_TABLE";
		category = "Supported SQL";
		break;
#	ifdef SQL_ANSI_SQL_DATETIME_LITERALS
	case SQL_ANSI_SQL_DATETIME_LITERALS:
		name = "SQL_ANSI_SQL_DATETIME_LITERALS";
		category = "Supported SQL";
		break;
#endif
	case SQL_ASYNC_MODE:
		name = "SQL_ASYNC_MODE";
		category = "Driver Information";
		break;
	case SQL_BATCH_ROW_COUNT:
		name = "SQL_BATCH_ROW_COUNT";
		category = "Driver Information";
		break;
	case SQL_BATCH_SUPPORT:
		name = "SQL_BATCH_SUPPORT";
		category = "Driver Information";
		break;
	case SQL_BOOKMARK_PERSISTENCE:
		name = "SQL_BOOKMARK_PERSISTENCE";
		category = "Data Source Information";
		break;
	case SQL_CATALOG_NAME:
		name = "SQL_CATALOG_NAME";
		category = "Supported SQL";
		break;
	case SQL_COLLATION_SEQ:
		name = "SQL_COLLATION_SEQ";
		category = "Data Source Information";
		break;
	case SQL_COLUMN_ALIAS:
		name = "SQL_COLUMN_ALIAS";
		category = "Supported SQL";
		break;
	case SQL_CONCAT_NULL_BEHAVIOR:
		name = "SQL_CONCAT_NULL_BEHAVIOR";
		category = "Data Source Information";
		break;
	case SQL_CONVERT_BIGINT:
		name = "SQL_CONVERT_BIGINT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_BINARY:
		name = "SQL_CONVERT_BINARY";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_BIT:
		name = "SQL_CONVERT_BIT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_CHAR:
		name = "SQL_CONVERT_CHAR";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_DATE:
		name = "SQL_CONVERT_DATE";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_DECIMAL:
		name = "SQL_CONVERT_DECIMAL";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_DOUBLE:
		name = "SQL_CONVERT_DOUBLE";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_FLOAT:
		name = "SQL_CONVERT_FLOAT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_FUNCTIONS:
		name = "SQL_CONVERT_FUNCTIONS";
		category = "Scalar Function Information";
		break;
	case SQL_CONVERT_INTEGER:
		name = "SQL_CONVERT_INTEGER";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_INTERVAL_DAY_TIME:
		name = "SQL_CONVERT_INTERVAL_DAY_TIME";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_INTERVAL_YEAR_MONTH:
		name = "SQL_CONVERT_INTERVAL_YEAR_MONTH";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_LONGVARBINARY:
		name = "SQL_CONVERT_LONGVARBINARY";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_LONGVARCHAR:
		name = "SQL_CONVERT_LONGVARCHAR";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_NUMERIC:
		name = "SQL_CONVERT_NUMERIC";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_REAL:
		name = "SQL_CONVERT_REAL";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_SMALLINT:
		name = "SQL_CONVERT_SMALLINT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_TIME:
		name = "SQL_CONVERT_TIME";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_TIMESTAMP:
		name = "SQL_CONVERT_TIMESTAMP";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_TINYINT:
		name = "SQL_CONVERT_TINYINT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_VARBINARY:
		name = "SQL_CONVERT_VARBINARY";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_VARCHAR:
		name = "SQL_CONVERT_VARCHAR";
		category = "Conversion Information";
		break;
	case SQL_CORRELATION_NAME:
		name = "SQL_CORRELATION_NAME";
		category = "Supported SQL";
		break;
	case SQL_CREATE_ASSERTION:
		name = "SQL_CREATE_ASSERTION";
		category = "Supported SQL";
		break;
	case SQL_CREATE_CHARACTER_SET:
		name = "SQL_CREATE_CHARACTER_SET";
		category = "Supported SQL";
		break;
	case SQL_CREATE_COLLATION:
		name = "SQL_CREATE_COLLATION";
		category = "Supported SQL";
		break;
	case SQL_CREATE_DOMAIN:
		name = "SQL_CREATE_DOMAIN";
		category = "Supported SQL";
		break;
	case SQL_CREATE_SCHEMA:
		name = "SQL_CREATE_SCHEMA";
		category = "Supported SQL";
		break;
	case SQL_CREATE_TABLE:
		name = "SQL_CREATE_TABLE";
		category = "Supported SQL";
		break;
	case SQL_CREATE_TRANSLATION:
		name = "SQL_CREATE_TRANSLATION";
		category = "Supported SQL";
		break;
	case SQL_CURSOR_COMMIT_BEHAVIOR:
		name = "SQL_CURSOR_COMMIT_BEHAVIOR";
		category = "Data Source Information";
		break;
	case SQL_CURSOR_ROLLBACK_BEHAVIOR:
		name = "SQL_CURSOR_ROLLBACK_BEHAVIOR";
		category = "Data Source Information";
		break;
	case SQL_CURSOR_SENSITIVITY:
		name = "SQL_CURSOR_SENSITIVITY";
		category = "Data Source Information";
		break;
	case SQL_DATABASE_NAME:
		name = "SQL_DATABASE_NAME";
		category = "DBMS Product Information";
		break;
	case SQL_DATA_SOURCE_NAME:
		name = "SQL_DATA_SOURCE_NAME";
		category = "Driver Information";
		break;
	case SQL_DATA_SOURCE_READ_ONLY:
		name = "SQL_DATA_SOURCE_READ_ONLY";
		category = "Data Source Information";
		break;
	case SQL_DBMS_NAME:
		name = "SQL_DBMS_NAME";
		category = "DBMS Product Information";
		break;
	case SQL_DBMS_VER:
		name = "SQL_DBMS_VER";
		category = "DBMS Product Information";
		break;
	case SQL_DDL_INDEX:
		name = "SQL_DDL_INDEX";
		category = "Supported SQL";
		break;
	case SQL_DEFAULT_TXN_ISOLATION:
		name = "SQL_DEFAULT_TXN_ISOLATION";
		category = "Data Source Information";
		break;
	case SQL_DESCRIBE_PARAMETER:
		name = "SQL_DESCRIBE_PARAMETER";
		category = "Data Source Information";
		break;
	case SQL_DM_VER:
		name = "SQL_DM_VER";
		category = "Added for ODBC 3.x";
		break;
	case SQL_DRIVER_HDBC:
		name = "SQL_DRIVER_HDBC";
		category = "Driver Information";
		break;
	case SQL_DRIVER_HDESC:
		name = "SQL_DRIVER_HDESC";
		category = "Driver Information";
		break;
	case SQL_DRIVER_HENV:
		name = "SQL_DRIVER_HENV";
		category = "Driver Information";
		break;
	case SQL_DRIVER_HLIB:
		name = "SQL_DRIVER_HLIB";
		category = "Driver Information";
		break;
	case SQL_DRIVER_HSTMT:
		name = "SQL_DRIVER_HSTMT";
		category = "Driver Information";
		break;
	case SQL_DRIVER_NAME:
		name = "SQL_DRIVER_NAME";
		category = "Driver Information";
		break;
	case SQL_DRIVER_ODBC_VER:
		name = "SQL_DRIVER_ODBC_VER";
		category = "Driver Information";
		break;
	case SQL_DRIVER_VER:
		name = "SQL_DRIVER_VER";
		category = "Driver Information";
		break;
	case SQL_DROP_ASSERTION:
		name = "SQL_DROP_ASSERTION";
		category = "Supported SQL";
		break;
	case SQL_DROP_CHARACTER_SET:
		name = "SQL_DROP_CHARACTER_SET";
		category = "Supported SQL";
		break;
	case SQL_DROP_COLLATION:
		name = "SQL_DROP_COLLATION";
		category = "Supported SQL";
		break;
	case SQL_DROP_DOMAIN:
		name = "SQL_DROP_DOMAIN";
		category = "Supported SQL";
		break;
	case SQL_DROP_SCHEMA:
		name = "SQL_DROP_SCHEMA";
		category = "Supported SQL";
		break;
	case SQL_DROP_TABLE:
		name = "SQL_DROP_TABLE";
		category = "Supported SQL";
		break;
	case SQL_DROP_TRANSLATION:
		name = "SQL_DROP_TRANSLATION";
		category = "Supported SQL";
		break;
	case SQL_DROP_VIEW:
		name = "SQL_DROP_VIEW";
		category = "Supported SQL";
		break;
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
		name = "SQL_DYNAMIC_CURSOR_ATTRIBUTES1";
		category = "Driver Information";
		break;
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
		name = "SQL_DYNAMIC_CURSOR_ATTRIBUTES2";
		category = "Driver Information";
		break;
	case SQL_EXPRESSIONS_IN_ORDERBY:
		name = "SQL_EXPRESSIONS_IN_ORDERBY";
		category = "Supported SQL";
		break;
	case SQL_FETCH_DIRECTION:
		name = "SQL_FETCH_DIRECTION";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_FILE_USAGE:
		name = "SQL_FILE_USAGE";
		category = "Driver Information";
		break;
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
		name = "SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1";
		category = "Driver Information";
		break;
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
		name = "SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2";
		category = "Driver Information";
		break;
	case SQL_GETDATA_EXTENSIONS:
		name = "SQL_GETDATA_EXTENSIONS";
		category = "Driver Information";
		break;
	case SQL_GROUP_BY:
		name = "SQL_GROUP_BY";
		category = "Supported SQL";
		break;
	case SQL_IDENTIFIER_CASE:
		name = "SQL_IDENTIFIER_CASE";
		category = "Supported SQL";
		break;
	case SQL_IDENTIFIER_QUOTE_CHAR:
		name = "SQL_IDENTIFIER_QUOTE_CHAR";
		category = "Supported SQL";
		break;
	case SQL_INDEX_KEYWORDS:
		name = "SQL_INDEX_KEYWORDS";
		category = "Supported SQL";
		break;
	case SQL_INFO_SCHEMA_VIEWS:
		name = "SQL_INFO_SCHEMA_VIEWS";
		category = "Driver Information";
		break;
	case SQL_INSERT_STATEMENT:
		name = "SQL_INSERT_STATEMENT";
		category = "Supported SQL";
		break;
	case SQL_KEYSET_CURSOR_ATTRIBUTES1:
		name = "SQL_KEYSET_CURSOR_ATTRIBUTES1";
		category = "Driver Information";
		break;
	case SQL_KEYSET_CURSOR_ATTRIBUTES2:
		name = "SQL_KEYSET_CURSOR_ATTRIBUTES2";
		category = "Driver Information";
		break;
	case SQL_KEYWORDS:
		name = "SQL_KEYWORDS";
		category = "Supported SQL";
		break;
	case SQL_LIKE_ESCAPE_CLAUSE:
		name = "SQL_LIKE_ESCAPE_CLAUSE";
		category = "Supported SQL";
		break;
	case SQL_LOCK_TYPES:
		name = "SQL_LOCK_TYPES";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
		name = "SQL_MAX_ASYNC_CONCURRENT_STATEMENTS";
		category = "Driver Information";
		break;
	case SQL_MAX_BINARY_LITERAL_LEN:
		name = "SQL_MAX_BINARY_LITERAL_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_CHAR_LITERAL_LEN:
		name = "SQL_MAX_CHAR_LITERAL_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMNS_IN_GROUP_BY:
		name = "SQL_MAX_COLUMNS_IN_GROUP_BY";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMNS_IN_INDEX:
		name = "SQL_MAX_COLUMNS_IN_INDEX";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMNS_IN_ORDER_BY:
		name = "SQL_MAX_COLUMNS_IN_ORDER_BY";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMNS_IN_SELECT:
		name = "SQL_MAX_COLUMNS_IN_SELECT";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMNS_IN_TABLE:
		name = "SQL_MAX_COLUMNS_IN_TABLE";
		category = "SQL Limits";
		break;
	case SQL_MAX_COLUMN_NAME_LEN:
		name = "SQL_MAX_COLUMN_NAME_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_CURSOR_NAME_LEN:
		name = "SQL_MAX_CURSOR_NAME_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_IDENTIFIER_LEN:
		name = "SQL_MAX_IDENTIFIER_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_INDEX_SIZE:
		name = "SQL_MAX_INDEX_SIZE";
		category = "SQL Limits";
		break;
	case SQL_MAX_OWNER_NAME_LEN:
		name = "SQL_MAX_SCHEMA_NAME_LEN/SQL_MAX_OWNER_NAME_LEN";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_MAX_PROCEDURE_NAME_LEN:
		name = "SQL_MAX_PROCEDURE_NAME_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_QUALIFIER_NAME_LEN:
		name = "SQL_MAX_CATALOG_NAME_LEN/SQL_MAX_QUALIFIER_NAME_LEN";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_MAX_ROW_SIZE:
		name = "SQL_MAX_ROW_SIZE";
		category = "SQL Limits";
		break;
	case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
		name = "SQL_MAX_ROW_SIZE_INCLUDES_LONG";
		category = "SQL Limits";
		break;
	case SQL_MAX_STATEMENT_LEN:
		name = "SQL_MAX_STATEMENT_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_TABLES_IN_SELECT:
		name = "SQL_MAX_TABLES_IN_SELECT";
		category = "SQL Limits";
		break;
	case SQL_MAX_TABLE_NAME_LEN:
		name = "SQL_MAX_TABLE_NAME_LEN";
		category = "SQL Limits";
		break;
	case SQL_MAX_USER_NAME_LEN:
		name = "SQL_MAX_USER_NAME_LEN";
		category = "SQL Limits";
		break;
	case SQL_MULTIPLE_ACTIVE_TXN:
		name = "SQL_MULTIPLE_ACTIVE_TXN";
		category = "Data Source Information";
		break;
	case SQL_MULT_RESULT_SETS:
		name = "SQL_MULT_RESULT_SETS";
		category = "Data Source Information";
		break;
	case SQL_NEED_LONG_DATA_LEN:
		name = "SQL_NEED_LONG_DATA_LEN";
		category = "Data Source Information";
		break;
	case SQL_NON_NULLABLE_COLUMNS:
		name = "SQL_NON_NULLABLE_COLUMNS";
		category = "Supported SQL";
		break;
	case SQL_NULL_COLLATION:
		name = "SQL_NULL_COLLATION";
		category = "Data Source Information";
		break;
	case SQL_NUMERIC_FUNCTIONS:
		name = "SQL_NUMERIC_FUNCTIONS";
		category = "Scalar Function Information";
		break;
	case SQL_ODBC_API_CONFORMANCE:
		name = "SQL_ODBC_API_CONFORMANCE";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_ODBC_INTERFACE_CONFORMANCE:
		name = "SQL_ODBC_INTERFACE_CONFORMANCE";
		category = "Driver Information";
		break;
	case SQL_ODBC_SQL_CONFORMANCE:
		name = "SQL_ODBC_SQL_CONFORMANCE";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_ODBC_SQL_OPT_IEF:
		name = "SQL_INTEGRITY/SQL_ODBC_SQL_OPT_IEF";
		category = "Renamed for ODBC 3.x";
		break;
#	ifdef SQL_ODBC_STANDARD_CLI_CONFORMANCE
	case SQL_ODBC_STANDARD_CLI_CONFORMANCE:
		name = "SQL_ODBC_STANDARD_CLI_CONFORMANCE";
		category = "Driver Information";
		break;
#	endif
	case SQL_ODBC_VER:
		name = "SQL_ODBC_VER";
		category = "Driver Information";
		break;
	case SQL_OJ_CAPABILITIES:
		name = "SQL_OJ_CAPABILITIES";
		category = "Supported SQL";
		break;
	case SQL_ORDER_BY_COLUMNS_IN_SELECT:
		name = "SQL_ORDER_BY_COLUMNS_IN_SELECT";
		category = "Supported SQL";
		break;
	case SQL_OUTER_JOINS:
		name = "SQL_OUTER_JOINS";
		category = "Supported SQL";
		break;
	case SQL_OWNER_TERM:
		name = "SQL_SCHEMA_TERM/SQL_OWNER_TERM";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_OWNER_USAGE:
		name = "SQL_SCHEMA_USAGE/SQL_OWNER_USAGE";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_PARAM_ARRAY_ROW_COUNTS:
		name = "SQL_PARAM_ARRAY_ROW_COUNTS";
		category = "Driver Information";
		break;
	case SQL_PARAM_ARRAY_SELECTS:
		name = "SQL_PARAM_ARRAY_SELECTS";
		category = "Driver Information";
		break;
	case SQL_POSITIONED_STATEMENTS:
		name = "SQL_POSITIONED_STATEMENTS";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_POS_OPERATIONS:
		name = "SQL_POS_OPERATIONS";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_PROCEDURES:
		name = "SQL_PROCEDURES";
		category = "Supported SQL";
		break;
	case SQL_PROCEDURE_TERM:
		name = "SQL_PROCEDURE_TERM";
		category = "Data Source Information";
		break;
	case SQL_QUALIFIER_LOCATION:
		name = "SQL_CATALOG_LOCATION/SQL_QUALIFIER_LOCATION";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_QUALIFIER_NAME_SEPARATOR:
		name = "SQL_CATALOG_NAME_SEPARATOR/SQL_QUALIFIER_NAME_SEPARATOR";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_QUALIFIER_TERM:
		name = "SQL_CATALOG_TERM/SQL_QUALIFIER_TERM";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_QUALIFIER_USAGE:
		name = "SQL_CATALOG_USAGE/SQL_QUALIFIER_USAGE";
		category = "Renamed for ODBC 3.x";
		break;
	case SQL_QUOTED_IDENTIFIER_CASE:
		name = "SQL_QUOTED_IDENTIFIER_CASE";
		category = "Supported SQL";
		break;
	case SQL_ROW_UPDATES:
		name = "SQL_ROW_UPDATES";
		category = "Driver Information";
		break;
	case SQL_SCROLL_CONCURRENCY:
		name = "SQL_SCROLL_CONCURRENCY";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_SCROLL_OPTIONS:
		name = "SQL_SCROLL_OPTIONS";
		category = "Data Source Information";
		break;
	case SQL_SEARCH_PATTERN_ESCAPE:
		name = "SQL_SEARCH_PATTERN_ESCAPE";
		category = "Driver Information";
		break;
	case SQL_SERVER_NAME:
		name = "SQL_SERVER_NAME";
		category = "Driver Information";
		break;
	case SQL_SPECIAL_CHARACTERS:
		name = "SQL_SPECIAL_CHARACTERS";
		category = "Supported SQL";
		break;
	case SQL_SQL_CONFORMANCE:
		name = "SQL_SQL_CONFORMANCE";
		category = "Supported SQL";
		break;
	case SQL_STATIC_CURSOR_ATTRIBUTES1:
		name = "SQL_STATIC_CURSOR_ATTRIBUTES1";
		category = "Driver Information";
		break;
	case SQL_STATIC_CURSOR_ATTRIBUTES2:
		name = "SQL_STATIC_CURSOR_ATTRIBUTES2";
		category = "Driver Information";
		break;
	case SQL_STATIC_SENSITIVITY:
		name = "SQL_STATIC_SENSITIVITY";
		category = "Deprecated in ODBC 3.x";
		break;
	case SQL_STRING_FUNCTIONS:
		name = "SQL_STRING_FUNCTIONS";
		category = "Scalar Function Information";
		break;
	case SQL_SUBQUERIES:
		name = "SQL_SUBQUERIES";
		category = "Supported SQL";
		break;
	case SQL_SYSTEM_FUNCTIONS:
		name = "SQL_SYSTEM_FUNCTIONS";
		category = "Scalar Function Information";
		break;
	case SQL_TABLE_TERM:
		name = "SQL_TABLE_TERM";
		category = "Data Source Information";
		break;
	case SQL_TIMEDATE_ADD_INTERVALS:
		name = "SQL_TIMEDATE_ADD_INTERVALS";
		category = "Scalar Function Information";
		break;
	case SQL_TIMEDATE_DIFF_INTERVALS:
		name = "SQL_TIMEDATE_DIFF_INTERVALS";
		category = "Scalar Function Information";
		break;
	case SQL_TIMEDATE_FUNCTIONS:
		name = "SQL_TIMEDATE_FUNCTIONS";
		category = "Scalar Function Information";
		break;
	case SQL_TXN_CAPABLE:
		name = "SQL_TXN_CAPABLE";
		category = "Data Source Information";
		break;
	case SQL_TXN_ISOLATION_OPTION:
		name = "SQL_TXN_ISOLATION_OPTION";
		category = "Data Source Information";
		break;
	case SQL_UNION:
		name = "SQL_UNION";
		category = "Supported SQL";
		break;
	case SQL_USER_NAME:
		name = "SQL_USER_NAME";
		category = "Data Source Information";
		break;
	case SQL_XOPEN_CLI_YEAR:
		name = "SQL_XOPEN_CLI_YEAR";
		category = "Added for ODBC 3.x";
		break;
	default:
		name = "unknown";
		category = "unknown";
		break;
	}

	tdsdump_log(TDS_DBG_INFO1, "odbc: not implemented: %s: option/type %d(%s) [category %s]\n", function_name, fType, name,
		    category);

	return;
}

#define ODBC_MAX_STAT_PARAM 8

static SQLRETURN
odbc_stat_execute(TDS_STMT * stmt, const char *begin, int nparams, ...)
{
	struct param
	{
		char *name;
		SQLCHAR *value;
		int len;
	}
	params[ODBC_MAX_STAT_PARAM];
	int i, len;
	char *proc, *p;
	int retcode;
	va_list marker;


	assert(nparams < ODBC_MAX_STAT_PARAM);

	/* read all params and calc len required */
	va_start(marker, nparams);
	len = strlen(begin) + 2;
	for (i = 0; i < nparams; ++i) {
		params[i].name = va_arg(marker, char *);

		params[i].value = va_arg(marker, SQLCHAR *);
		params[i].len = odbc_get_string_size(va_arg(marker, int), params[i].value);

		len += strlen(params[i].name) + tds_quote_string(stmt->hdbc->tds_socket, NULL, params[i].value, params[i].len) + 3;
	}
	va_end(marker);

	/* allocate space for string */
	if (!(proc = (char *) malloc(len))) {
		odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}

	/* build string */
	p = proc;
	strcpy(p, begin);
	p += strlen(begin);
	for (i = 0; i < nparams; ++i) {
		if (params[i].len <= 0)
			continue;
		strcpy(p, params[i].name);
		p += strlen(params[i].name);
		*p++ = '=';
		p += tds_quote_string(stmt->hdbc->tds_socket, p, params[i].value, params[i].len);
		*p++ = ',';
	}
	*--p = '\0';

	/* set it */
	retcode = odbc_set_stmt_query(stmt, proc, p - proc);
	free(proc);

	if (retcode != SQL_SUCCESS)
		return retcode;

	/* execute it */
	retcode = _SQLExecute(stmt);
	if (SQL_SUCCEEDED(retcode))
		odbc_upper_column_names(stmt);

	return retcode;
}
