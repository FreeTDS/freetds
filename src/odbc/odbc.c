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

static char software_version[] = "$Id: odbc.c,v 1.117 2003-01-07 14:42:50 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR * phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR * phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR * phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);
static char *strncpy_null(char *dst, const char *src, int len);
static int sql_to_c_type_default(int sql_type);
static int mymessagehandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg);
static int myerrorhandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg);


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

/* spinellia@acm.org : copied shamelessly from change_database */
static SQLRETURN
change_autocommit(TDS_DBC * dbc, int state)
{
	SQLRETURN ret;
	TDSSOCKET *tds;
	char query[80];
	TDS_INT result_type;

	tds = dbc->tds_socket;

	/* mssql: SET IMPLICIT_TRANSACTION ON
	 * sybase: SET CHAINED ON */

	/* implicit transactions are on if autocommit is off :-| */
	if (TDS_IS_MSSQL(tds))
		sprintf(query, "set implicit_transactions %s", (state ? "off" : "on"));
	else
		sprintf(query, "set chained %s", (state ? "off" : "on"));

	tdsdump_log(TDS_DBG_INFO1, "change_autocommit: executing %s\n", query);

	ret = tds_submit_query(tds, query);
	if (ret != TDS_SUCCEED) {
		odbc_errs_add(&dbc->errs, ODBCERR_GENERIC, "Could not change transaction status");
		return SQL_ERROR;
	}

	if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
		return SQL_ERROR;

	dbc->autocommit_state = state;
	return SQL_SUCCESS;
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
	tds_fix_connect(connect_info);

	/* fix login type */
	if (!connect_info->try_domain_login) {
		if (strchr(connect_info->user_name, '\\')) {
			connect_info->try_domain_login = 1;
			connect_info->try_server_login = 0;
		}
	}
	if (!connect_info->try_domain_login && !connect_info->try_server_login)
		connect_info->try_server_login = 1;

	if (tds_connect(dbc->tds_socket, connect_info) == TDS_FAIL) {
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

SQLRETURN SQL_API
SQLColumnPrivileges(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		    SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLCHAR FAR * szColumnName,
		    SQLSMALLINT cbColumnName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLColumnPrivileges: function not implemented");
	return SQL_ERROR;
}

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

SQLRETURN SQL_API
SQLForeignKeys(SQLHSTMT hstmt, SQLCHAR FAR * szPkCatalogName, SQLSMALLINT cbPkCatalogName, SQLCHAR FAR * szPkSchemaName,
	       SQLSMALLINT cbPkSchemaName, SQLCHAR FAR * szPkTableName, SQLSMALLINT cbPkTableName, SQLCHAR FAR * szFkCatalogName,
	       SQLSMALLINT cbFkCatalogName, SQLCHAR FAR * szFkSchemaName, SQLSMALLINT cbFkSchemaName, SQLCHAR FAR * szFkTableName,
	       SQLSMALLINT cbFkTableName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLForeignKeys: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLMoreResults(SQLHSTMT hstmt)
{
	TDSSOCKET *tds;
	TDS_INT result_type;

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	/* try to go to the next recordset */
	for (;;) {
		switch (tds_process_result_tokens(tds, &result_type)) {
		case TDS_NO_MORE_RESULTS:
			return SQL_NO_DATA_FOUND;
		case TDS_SUCCEED:
			switch (result_type) {
			case TDS_COMPUTE_RESULT:
			case TDS_ROW_RESULT:
			case TDS_CMD_FAIL:
				/* FIXME this row is used only as a flag for update binding, should be cleared if binding/result chenged */
				stmt->row = 0;
				return SQL_SUCCESS;

			case TDS_STATUS_RESULT:
				odbc_set_return_status(stmt);
				break;

				/* ?? */
			case TDS_CMD_DONE:
				/* TODO: correct ?? */
				if (tds->res_info) {
					stmt->row = 0;
					return SQL_SUCCESS;
				}
			case TDS_PARAM_RESULT:
			case TDS_COMPUTEFMT_RESULT:
			case TDS_MSG_RESULT:
			case TDS_ROWFMT_RESULT:
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

SQLRETURN SQL_API
SQLPrimaryKeys(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	       SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLPrimaryKeys: function not implemented");
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLProcedureColumns(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		    SQLSMALLINT cbSchemaName, SQLCHAR FAR * szProcName, SQLSMALLINT cbProcName, SQLCHAR FAR * szColumnName,
		    SQLSMALLINT cbColumnName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLProcedureColumns: function not implemented");
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLProcedures(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	      SQLSMALLINT cbSchemaName, SQLCHAR FAR * szProcName, SQLSMALLINT cbProcName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLProcedures: function not implemented");
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLSetPos(SQLHSTMT hstmt, SQLUSMALLINT irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLSetPos: function not implemented");
	return SQL_ERROR;
}

SQLRETURN SQL_API
SQLTablePrivileges(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
		   SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLTablePrivileges: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLSetEnvAttr(SQLHENV henv, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER StringLength)
{
	INIT_HENV;
	switch (Attribute) {
	case SQL_ATTR_ODBC_VERSION:
		switch ((SQLINTEGER) Value) {
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
		cur->param_bindtype = sql_to_c_type_default(fSqlType);
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

	memset(dbc, '\0', sizeof(TDS_DBC));
	dbc->henv = env;
	dbc->tds_login = tds_alloc_login();
	if (!dbc->tds_login) {
		free(dbc);
		odbc_errs_add(&env->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}
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
	ctx->locale->date_fmt = strdup("%Y-%m-%d");

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
	if (icol <= 0 || icol > tds->res_info->num_cols) {
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
		*pfSqlType = odbc_get_client_type(colinfo->column_type, colinfo->column_size);
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
		*pfDesc = odbc_get_client_type(colinfo->column_type, colinfo->column_size);
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
		switch (odbc_get_client_type(colinfo->column_type, colinfo->column_size)) {
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
				    icol, odbc_get_client_type(colinfo->column_type, colinfo->column_size)
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
	char *p;
	struct _sql_errors *errs = NULL;
	TDS_DBC *dbc;

	if (asprintf(&p,
		     " Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
		     msg->msg_number, msg->msg_level, msg->msg_state, msg->server, msg->line_number, msg->message) < 0)
		return 0;
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
		odbc_errs_add(errs, ODBCERR_GENERIC, p);
	free(p);
	return 1;
}

static int
myerrorhandler(TDSCONTEXT * ctx, TDSSOCKET * tds, TDSMSGINFO * msg)
{
	char *p;
	struct _sql_errors *errs = NULL;
	TDS_DBC *dbc;

	if (asprintf(&p,
		     " Err %d, Level %d, State %d, Server %s, Line %d\n%s\n",
		     msg->msg_number, msg->msg_level, msg->msg_state, msg->server, msg->line_number, msg->message) < 0)
		return 0;
	if (tds && tds->parent) {
		dbc = (TDS_DBC *) tds->parent;
		errs = &dbc->errs;
		if (dbc->current_statement)
			errs = &dbc->current_statement->errs;
	} else if (ctx->parent) {
		errs = &((TDS_ENV *) ctx->parent)->errs;
	}
	if (errs)
		odbc_errs_add(errs, ODBCERR_GENERIC, p);
	free(p);
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

	if (!(tds_submit_query(tds, stmt->query) == TDS_SUCCEED)) {
/*        odbc_LogError (tds->msg_info->message); */
		return SQL_ERROR;
	}
	stmt->hdbc->current_statement = stmt;

	/* TODO review this, ODBC return parameter in other way, for compute I don't know */
	while ((ret = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_COMPUTE_RESULT:
		case TDS_PARAM_RESULT:
		case TDS_ROW_RESULT:
		case TDS_STATUS_RESULT:
			done = 1;
			break;
		case TDS_CMD_FAIL:
			result = SQL_ERROR;
			done = 1;
			break;

		case TDS_CMD_DONE:
			if (tds->res_info)
				done = 1;
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
		if (result == SQL_SUCCESS && stmt->errs.num_errors != 0)
			return SQL_SUCCESS_WITH_INFO;
		return result;
	} else if (ret == TDS_SUCCEED) {
		if (result == SQL_SUCCESS && stmt->errs.num_errors != 0)
			return SQL_SUCCESS_WITH_INFO;
		return result;
	} else {
		tdsdump_log(TDS_DBG_INFO1, "SQLExecute: bad results\n");
		return result;
	}
}

SQLRETURN SQL_API
SQLExecDirect(SQLHSTMT hstmt, SQLCHAR FAR * szSqlStr, SQLINTEGER cbSqlStr)
{
	INIT_HSTMT;

	stmt->param_count = 0;
	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, (char *) szSqlStr, cbSqlStr))
		return SQL_ERROR;
	if (SQL_SUCCESS != prepare_call(stmt))
		return SQL_ERROR;

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

	/* translate to native format */
	if (SQL_SUCCESS != prepare_call(stmt))
		return SQL_ERROR;

#ifdef ENABLE_DEVELOPING
	tds = stmt->hdbc->tds_socket;

	if (stmt->param_count > 0) {
		/* prepare dynamic query (only for first SQLExecute call) */
		if (!stmt->dyn) {
			TDS_INT result_type;

			tdsdump_log(TDS_DBG_INFO1, "Creating prepared statement\n");
			if (tds_submit_prepare(tds, stmt->prepared_query, NULL, &stmt->dyn) == TDS_FAIL)
				return SQL_ERROR;
			if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
				return SQL_ERROR;
		}
		/* build parameters list */
		dyn = stmt->dyn;
		/* TODO rebuild should be done for every bingings change */
		/*if (dyn->num_params != stmt->param_count) */  {
			int i;
			TDSPARAMINFO *params;
			TDSCOLINFO *curcol;

			tds_free_input_params(dyn);
			tdsdump_log(TDS_DBG_INFO1, "Setting input parameters\n");
			for (i = 0; i < (int) stmt->param_count; ++i) {
				param = odbc_find_param(stmt, i + 1);
				if (!param)
					return SQL_ERROR;
				if (!(params = tds_alloc_param_result(dyn->params))) {
					odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
					return SQL_ERROR;
				}
				dyn->params = params;
				/* add another type and copy data */
				curcol = params->columns[i];
				sql2tds(stmt->hdbc->henv->tds_ctx, param, params, curcol);
			}
		}
		tdsdump_log(TDS_DBG_INFO1, "End prepare, execute\n");
		/* TODO check errors */
		if (tds_submit_execute(tds, dyn) == TDS_FAIL)
			return SQL_ERROR;

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
	/* TODO why don't free anything ??? */
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
		if (tds->state == TDS_PENDING && stmt->hdbc->current_statement == stmt) {
			tds_send_cancel(tds);
			tds_process_cancel(tds);
		}
	}

	/* free it */
	if (fOption == SQL_DROP) {
		if (stmt->query)
			free(stmt->query);
		if (stmt->prepared_query)
			free(stmt->prepared_query);
		odbc_errs_reset(&stmt->errs);
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

SQLRETURN SQL_API
SQLGetStmtAttr(SQLHSTMT hstmt, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER BufferLength, SQLINTEGER * StringLength)
{
	INIT_HSTMT;

	if (BufferLength == SQL_IS_UINTEGER) {
		return SQLGetStmtOption(hstmt, Attribute, Value);
	} else {
		odbc_errs_add(&stmt->errs, ODBCERR_INVALIDOPTION, NULL);
		return SQL_ERROR;
	}
}

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

	return SQL_SUCCESS;
}

SQLRETURN SQL_API
SQLRowCount(SQLHSTMT hstmt, SQLINTEGER FAR * pcrow)
{
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	INIT_HSTMT;

/* 7/28/2001 begin l@poliris.com */
	tds = stmt->hdbc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo == NULL) {
		*pcrow = 0;
		return SQL_SUCCESS;
/*
        if (tds && tds->msg_info && tds->msg_info->message)
            odbc_LogError (tds->msg_info->message);
        else
            odbc_LogError ("SQLRowCount: resinfo is NULL");
    
        return SQL_ERROR;
*/
	}
	*pcrow = resinfo->row_count;

/* end l@poliris.com */

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
	SQLRETURN ret;
	TDSSOCKET *tds;
	char query[256];
	SQLRETURN cc = SQL_SUCCESS;
	TDS_INT result_type;

	tdsdump_log(TDS_DBG_INFO1, "change_transaction(0x%x,%d)\n", dbc, state);

	tds = dbc->tds_socket;
	strcpy(query, (state ? "commit" : "rollback"));
	ret = tds_submit_query(tds, query);
	if (ret != TDS_SUCCEED) {
		odbc_errs_add(&dbc->errs, ODBCERR_GENERIC, "Could not perform COMMIT or ROLLBACK");
		cc = SQL_ERROR;
	}

	if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL)
		return SQL_ERROR;

	return cc;
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
	char szQuery[4096];
	int nTableName = odbc_get_string_size(cbTableName, szTableName);
	int nTableOwner = odbc_get_string_size(cbSchemaName, szSchemaName);
	int nTableQualifier = odbc_get_string_size(cbCatalogName, szCatalogName);
	int nColumnName = odbc_get_string_size(cbColumnName, szColumnName);
	int bNeedComma = 0;

	INIT_HSTMT;

	sprintf(szQuery, "exec sp_columns ");

	if (nTableName) {
		strcat(szQuery, "@table_name = '");
		strncat(szQuery, (const char *) szTableName, nTableName);
		strcat(szQuery, "'");
		bNeedComma = 1;
	}

	if (nTableOwner) {
		if (bNeedComma)
			strcat(szQuery, ", ");
		strcat(szQuery, "@table_owner = '");
		strncat(szQuery, (const char *) szSchemaName, nTableOwner);
		strcat(szQuery, "'");
		bNeedComma = 1;
	}

	if (nTableQualifier) {
		if (bNeedComma)
			strcat(szQuery, ", ");
		strcat(szQuery, "@table_qualifier = '");
		strncat(szQuery, (const char *) szCatalogName, nTableQualifier);
		strcat(szQuery, "'");
		bNeedComma = 1;
	}

	if (nColumnName) {
		if (bNeedComma)
			strcat(szQuery, ", ");
		strcat(szQuery, "@column_name = '");
		strncat(szQuery, (const char *) szColumnName, nColumnName);
		strcat(szQuery, "'");
		bNeedComma = 1;
	}

	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, szQuery, strlen(szQuery)))
		return SQL_ERROR;

	return _SQLExecute(stmt);
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
			src = ((TDSBLOBINFO *) src)->textvalue + colinfo->column_text_sqlgetdatapos;
			srclen = colinfo->column_cur_size - colinfo->column_text_sqlgetdatapos;
		} else {
			srclen = colinfo->column_cur_size;
		}
		nSybType = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
		*pcbValue = convert_tds2sql(context, nSybType, src, srclen, fCType, (TDS_CHAR *) rgbValue, cbValueMax);

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

static void
_set_func_exists(SQLUSMALLINT FAR * pfExists, SQLUSMALLINT fFunction)
{
	SQLUSMALLINT FAR *mod;

	mod = pfExists + (fFunction >> 4);
	*mod |= (1 << (fFunction & 0x0f));
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
		
#define FUNC(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define NO_FUNC(n)
#define FUNC3(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define NO_FUNC3(n)
		FUNC(SQL_API_SQLALLOCCONNECT);
		FUNC(SQL_API_SQLALLOCENV);
		FUNC3(SQL_API_SQLALLOCHANDLE);
		FUNC(SQL_API_SQLALLOCSTMT);
		FUNC(SQL_API_SQLBINDCOL);
		FUNC(SQL_API_SQLBINDPARAMETER);
		NO_FUNC(SQL_API_SQLBROWSECONNECT);
		NO_FUNC3(SQL_API_SQLBULKOPERATIONS);
		FUNC(SQL_API_SQLCANCEL);
		NO_FUNC3(SQL_API_SQLCLOSECURSOR);
		NO_FUNC3(SQL_API_SQLCOLATTRIBUTE);
		FUNC(SQL_API_SQLCOLATTRIBUTES);
		NO_FUNC(SQL_API_SQLCOLUMNPRIVILEGES);
		FUNC(SQL_API_SQLCOLUMNS);
		FUNC(SQL_API_SQLCONNECT);
		NO_FUNC3(SQL_API_SQLCOPYDESC);
		FUNC(SQL_API_SQLDESCRIBECOL);
		NO_FUNC(SQL_API_SQLDESCRIBEPARAM);
		FUNC(SQL_API_SQLDISCONNECT);
		FUNC(SQL_API_SQLDRIVERCONNECT);
		NO_FUNC3(SQL_API_SQLENDTRAN);
		FUNC(SQL_API_SQLERROR);
		FUNC(SQL_API_SQLEXECDIRECT);
		FUNC(SQL_API_SQLEXECUTE);
		NO_FUNC(SQL_API_SQLEXTENDEDFETCH);
		FUNC(SQL_API_SQLFETCH);
		NO_FUNC3(SQL_API_SQLFETCHSCROLL);
		NO_FUNC(SQL_API_SQLFOREIGNKEYS);
		FUNC(SQL_API_SQLFREECONNECT);
		FUNC(SQL_API_SQLFREEENV);
		FUNC3(SQL_API_SQLFREEHANDLE);
		FUNC(SQL_API_SQLFREESTMT);
		NO_FUNC3(SQL_API_SQLGETCONNECTATTR);
		FUNC(SQL_API_SQLGETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLGETCURSORNAME);
		FUNC(SQL_API_SQLGETDATA);
		NO_FUNC3(SQL_API_SQLGETDESCFIELD);
		NO_FUNC3(SQL_API_SQLGETDESCREC);
		FUNC3(SQL_API_SQLGETDIAGFIELD);
		FUNC3(SQL_API_SQLGETDIAGREC);
		NO_FUNC3(SQL_API_SQLGETENVATTR);
		FUNC(SQL_API_SQLGETFUNCTIONS);
		FUNC(SQL_API_SQLGETINFO);
		FUNC3(SQL_API_SQLGETSTMTATTR);
		FUNC(SQL_API_SQLGETSTMTOPTION);
		FUNC(SQL_API_SQLGETTYPEINFO);
		FUNC(SQL_API_SQLMORERESULTS);
		NO_FUNC(SQL_API_SQLNATIVESQL);
		FUNC(SQL_API_SQLNUMPARAMS);
		FUNC(SQL_API_SQLNUMRESULTCOLS);
		FUNC(SQL_API_SQLPARAMDATA);
		NO_FUNC(SQL_API_SQLPARAMOPTIONS);
		FUNC(SQL_API_SQLPREPARE);
		NO_FUNC(SQL_API_SQLPRIMARYKEYS);
		NO_FUNC(SQL_API_SQLPROCEDURECOLUMNS);
		NO_FUNC(SQL_API_SQLPROCEDURES);
		FUNC(SQL_API_SQLPUTDATA);
		FUNC(SQL_API_SQLROWCOUNT);
		FUNC3(SQL_API_SQLSETCONNECTATTR);
		FUNC(SQL_API_SQLSETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLSETCURSORNAME);
		NO_FUNC3(SQL_API_SQLSETDESCFIELD);
		NO_FUNC3(SQL_API_SQLSETDESCREC);
		FUNC3(SQL_API_SQLSETENVATTR);
		NO_FUNC(SQL_API_SQLSETPARAM);
		NO_FUNC(SQL_API_SQLSETPOS);
		NO_FUNC(SQL_API_SQLSETSCROLLOPTIONS);
		NO_FUNC3(SQL_API_SQLSETSTMTATTR);
		FUNC(SQL_API_SQLSETSTMTOPTION);
		NO_FUNC(SQL_API_SQLSPECIALCOLUMNS);
		NO_FUNC(SQL_API_SQLSTATISTICS);
		NO_FUNC(SQL_API_SQLTABLEPRIVILEGES);
		FUNC(SQL_API_SQLTABLES);
		FUNC(SQL_API_SQLTRANSACT);
#undef FUNC
#undef NO_FUNC
#undef FUNC3
#undef NO_FUNC3
		return SQL_SUCCESS;
#endif

	case SQL_API_ALL_FUNCTIONS:
		tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetFunctions: " "fFunction is SQL_API_ALL_FUNCTIONS\n");
		for (i = 0; i < 100; ++i) {
			pfExists[i] = 0;
		}
		
#define FUNC(n) if (n >= 0 && n <= 800) pfExists[n/16] |= (1 << n%16);
#define NO_FUNC(n)
		FUNC(SQL_API_SQLALLOCCONNECT);
		FUNC(SQL_API_SQLALLOCENV);
		FUNC(SQL_API_SQLALLOCSTMT);
		FUNC(SQL_API_SQLBINDCOL);
		FUNC(SQL_API_SQLBINDPARAMETER);
		FUNC(SQL_API_SQLCANCEL);
		FUNC(SQL_API_SQLCOLATTRIBUTES);
		FUNC(SQL_API_SQLCOLUMNS);
		FUNC(SQL_API_SQLCONNECT);
		FUNC(SQL_API_SQLDRIVERCONNECT);
		FUNC(SQL_API_SQLDESCRIBECOL);
		FUNC(SQL_API_SQLDISCONNECT);
		FUNC(SQL_API_SQLERROR);
		FUNC(SQL_API_SQLEXECDIRECT);
		FUNC(SQL_API_SQLEXECUTE);
		FUNC(SQL_API_SQLFETCH);
		FUNC(SQL_API_SQLFREECONNECT);
		FUNC(SQL_API_SQLFREEENV);
		FUNC(SQL_API_SQLFREESTMT);
		FUNC(SQL_API_SQLGETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLGETCURSORNAME);
		FUNC(SQL_API_SQLGETDATA);
		FUNC(SQL_API_SQLGETFUNCTIONS);
		FUNC(SQL_API_SQLGETINFO);
		FUNC(SQL_API_SQLGETSTMTOPTION);
		FUNC(SQL_API_SQLGETTYPEINFO);
		FUNC(SQL_API_SQLMORERESULTS);
		FUNC(SQL_API_SQLNUMPARAMS);
		FUNC(SQL_API_SQLNUMRESULTCOLS);
		FUNC(SQL_API_SQLPARAMDATA);
		FUNC(SQL_API_SQLPREPARE);
		FUNC(SQL_API_SQLPUTDATA);
		FUNC(SQL_API_SQLROWCOUNT);
		FUNC(SQL_API_SQLSETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLSETCURSORNAME);
		NO_FUNC(SQL_API_SQLSETPARAM);
		FUNC(SQL_API_SQLSETSTMTOPTION);
		NO_FUNC(SQL_API_SQLSPECIALCOLUMNS);
		NO_FUNC(SQL_API_SQLSTATISTICS);
		FUNC(SQL_API_SQLTABLES);
		FUNC(SQL_API_SQLTRANSACT);
#undef FUNC
#undef NO_FUNC
#undef FUNC3
#undef NO_FUNC3
		return SQL_SUCCESS;
		break;
#define FUNC(n) case n:
#define NO_FUNC(n)
		FUNC(SQL_API_SQLALLOCCONNECT);
		FUNC(SQL_API_SQLALLOCENV);
		FUNC(SQL_API_SQLALLOCSTMT);
		FUNC(SQL_API_SQLBINDCOL);
		FUNC(SQL_API_SQLBINDPARAMETER);
		FUNC(SQL_API_SQLCANCEL);
		FUNC(SQL_API_SQLCOLATTRIBUTES);
		FUNC(SQL_API_SQLCOLUMNS);
		FUNC(SQL_API_SQLCONNECT);
		FUNC(SQL_API_SQLDRIVERCONNECT);
		FUNC(SQL_API_SQLDESCRIBECOL);
		FUNC(SQL_API_SQLDISCONNECT);
		FUNC(SQL_API_SQLERROR);
		FUNC(SQL_API_SQLEXECDIRECT);
		FUNC(SQL_API_SQLEXECUTE);
		FUNC(SQL_API_SQLFETCH);
		FUNC(SQL_API_SQLFREECONNECT);
		FUNC(SQL_API_SQLFREEENV);
		FUNC(SQL_API_SQLFREESTMT);
		FUNC(SQL_API_SQLGETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLGETCURSORNAME);
		FUNC(SQL_API_SQLGETDATA);
		FUNC(SQL_API_SQLGETFUNCTIONS);
		FUNC(SQL_API_SQLGETINFO);
		FUNC(SQL_API_SQLGETSTMTOPTION);
		FUNC(SQL_API_SQLGETTYPEINFO);
		FUNC(SQL_API_SQLMORERESULTS);
		FUNC(SQL_API_SQLNUMPARAMS);
		FUNC(SQL_API_SQLNUMRESULTCOLS);
		FUNC(SQL_API_SQLPARAMDATA);
		FUNC(SQL_API_SQLPREPARE);
		FUNC(SQL_API_SQLPUTDATA);
		FUNC(SQL_API_SQLROWCOUNT);
		FUNC(SQL_API_SQLSETCONNECTOPTION);
		NO_FUNC(SQL_API_SQLSETCURSORNAME);
		NO_FUNC(SQL_API_SQLSETPARAM);
		FUNC(SQL_API_SQLSETSTMTOPTION);
		NO_FUNC(SQL_API_SQLSPECIALCOLUMNS);
		NO_FUNC(SQL_API_SQLSTATISTICS);
		FUNC(SQL_API_SQLTABLES);
		FUNC(SQL_API_SQLTRANSACT);
#if (ODBCVER >= 0x300)
		FUNC(SQL_API_SQLALLOCHANDLE);
		NO_FUNC(SQL_API_SQLCLOSECURSOR);
		NO_FUNC(SQL_API_SQLCOPYDESC);
		NO_FUNC(SQL_API_SQLENDTRAN);
		NO_FUNC(SQL_API_SQLFETCHSCROLL);
		NO_FUNC(SQL_API_SQLGETCONNECTATTR);
		FUNC(SQL_API_SQLFREEHANDLE);
		NO_FUNC(SQL_API_SQLGETDESCFIELD);
		NO_FUNC(SQL_API_SQLGETDESCREC);
		FUNC(SQL_API_SQLGETDIAGFIELD);
		FUNC(SQL_API_SQLGETDIAGREC);
		NO_FUNC(SQL_API_SQLGETENVATTR);
		FUNC(SQL_API_SQLGETSTMTATTR);
		FUNC(SQL_API_SQLSETCONNECTATTR);
		NO_FUNC(SQL_API_SQLSETDESCFIELD);
		NO_FUNC(SQL_API_SQLSETDESCREC);
		FUNC(SQL_API_SQLSETENVATTR);
		NO_FUNC(SQL_API_SQLSETSTMTATTR);
#endif
#undef FUNC
#undef NO_FUNC
#undef FUNC3
#undef NO_FUNC3
		*pfExists = 1;	/* SQL_TRUE */
		return SQL_SUCCESS;
	default:
		*pfExists = 0;
		return SQL_SUCCESS;
		break;
	}
	return SQL_SUCCESS;
}


/* pwillia6@csc.com.au 01/25/02 */
/* strncpy copies up to len characters, and doesn't terminate */
/* the destination string if src has len characters or more. */
/* instead, I want it to copy up to len-1 characters and always */
/* terminate the destination string. */
static char *
strncpy_null(char *dst, const char *src, int len)
{
	int i;


	if (NULL != dst) {
		/*  Just in case, check for special lengths */
		if (len == SQL_NULL_DATA) {
			dst[0] = '\0';
			return NULL;
		} else if (len == SQL_NTS)
			len = strlen(src) + 1;

		for (i = 0; src[i] && i < len - 1; i++) {
			dst[i] = src[i];
		}

		if (len > 0) {
			dst[i] = '\0';
		}
	}
	return dst;
}

SQLRETURN SQL_API
SQLGetInfo(SQLHDBC hdbc, SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue, SQLSMALLINT cbInfoValueMax,
	   SQLSMALLINT FAR * pcbInfoValue)
{
	const char *p = NULL;
	SQLSMALLINT *siInfoValue = (SQLSMALLINT *) rgbInfoValue;
	SQLUINTEGER *uiInfoValue = (SQLUINTEGER *) rgbInfoValue;

	INIT_HDBC;

	switch (fInfoType) {
		/* TODO dbms name and version can be safed from login... */
	case SQL_DBMS_NAME:
		p = "SQL Server";
		break;
	case SQL_DBMS_VER:
		p = "unknown version";
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
	case SQL_ACTIVE_STATEMENTS:
		*siInfoValue = 1;
		break;
	case SQL_SCROLL_OPTIONS:
		*uiInfoValue = SQL_SO_FORWARD_ONLY | SQL_SO_STATIC;
		break;
	case SQL_SCROLL_CONCURRENCY:
		*uiInfoValue = SQL_SCCO_READ_ONLY;
		break;
	case SQL_TXN_CAPABLE:
		/* transaction for DML and DDL */
		*siInfoValue = SQL_TC_ALL;
		break;
	case SQL_DEFAULT_TXN_ISOLATION:
		*uiInfoValue = SQL_TXN_READ_COMMITTED;
		break;
	case SQL_FILE_USAGE:
		*uiInfoValue = SQL_FILE_NOT_SUPPORTED;
		break;
	case SQL_ALTER_TABLE:
		*uiInfoValue = SQL_AT_ADD_COLUMN | SQL_AT_ADD_COLUMN_DEFAULT
			| SQL_AT_ADD_COLUMN_SINGLE | SQL_AT_ADD_CONSTRAINT
			| SQL_AT_ADD_TABLE_CONSTRAINT | SQL_AT_CONSTRAINT_NAME_DEFINITION | SQL_AT_DROP_COLUMN_RESTRICT;
		break;
	case SQL_DATA_SOURCE_READ_ONLY:
		/* TODO: determine the right answer from connection 
		 * attribute SQL_ATTR_ACCESS_MODE */
		*uiInfoValue = 0;	/* false, writable */
		break;

		/* TODO support for other options */
	default:
		tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetInfo: " "info option %d not supported\n", fInfoType);
		odbc_errs_add(&dbc->errs, ODBCERR_NOTIMPLEMENTED, "Option not supported");
		return SQL_ERROR;
	}

	if (p) {		/* char/binary data */
		int len = strlen(p);

		if (rgbInfoValue) {
			strncpy_null((char *) rgbInfoValue, p, (size_t) cbInfoValueMax);

			if (len >= cbInfoValueMax) {
				odbc_errs_add(&dbc->errs, ODBCERR_DATATRUNCATION, NULL);
				return (SQL_SUCCESS_WITH_INFO);
			}
		}
		if (pcbInfoValue)
			*pcbInfoValue = len;
	}

	return SQL_SUCCESS;
}

/* end pwillia6@csc.com.au 01/25/02 */

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
odbc_upper_column_names(TDSSOCKET * tds)
{
	TDSRESULTINFO *resinfo;
	TDSCOLINFO *colinfo;
	int icol;
	char *p;

	if (!tds->res_info)
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

	INIT_HSTMT;

	tds = stmt->hdbc->tds_socket;

	/* For MSSQL6.5 and Sybase 11.9 sp_datatype_info work */
	/* FIXME what about early Sybase products ? */
	if (!fSqlType) {
		static const char *sql = "EXEC sp_datatype_info";

		if (SQL_SUCCESS != odbc_set_stmt_query(stmt, sql, strlen(sql)))
			return SQL_ERROR;
	} else {
		static const char sql_templ[] = "EXEC sp_datatype_info %d";
		char sql[sizeof(sql_templ) + 20];

		sprintf(sql, sql_templ, fSqlType);
		if (SQL_SUCCESS != odbc_set_stmt_query(stmt, sql, strlen(sql)))
			return SQL_ERROR;
	}

      redo:
	res = _SQLExecute(stmt);

	odbc_upper_column_names(stmt->hdbc->tds_socket);

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
	SQLUINTEGER u_value = (SQLUINTEGER) ValuePtr;

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

SQLRETURN SQL_API
SQLStatistics(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	      SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLUSMALLINT fUnique,
	      SQLUSMALLINT fAccuracy)
{
	INIT_HSTMT;
	odbc_errs_add(&stmt->errs, ODBCERR_NOTIMPLEMENTED, "SQLStatistics: function not implemented");
	return SQL_ERROR;
}
#endif

SQLRETURN SQL_API
SQLTables(SQLHSTMT hstmt, SQLCHAR FAR * szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR FAR * szSchemaName,
	  SQLSMALLINT cbSchemaName, SQLCHAR FAR * szTableName, SQLSMALLINT cbTableName, SQLCHAR FAR * szTableType,
	  SQLSMALLINT cbTableType)
{
	char *query, *p;
	static const char sptables[] = "exec sp_tables ";
	int querylen, clen, slen, tlen, ttlen;
	int first = 1;
	SQLRETURN result;

	INIT_HSTMT;

	clen = odbc_get_string_size(cbCatalogName, szCatalogName);
	slen = odbc_get_string_size(cbSchemaName, szSchemaName);
	tlen = odbc_get_string_size(cbTableName, szTableName);
	ttlen = odbc_get_string_size(cbTableType, szTableType);

	querylen = strlen(sptables) + clen + slen + tlen + ttlen + 40;	/* a little padding for quotes and commas */
	query = (char *) malloc(querylen);
	if (!query) {
		odbc_errs_add(&stmt->errs, ODBCERR_MEMORY, NULL);
		return SQL_ERROR;
	}
	p = query;

	strcpy(p, sptables);
	p += sizeof(sptables) - 1;

	if (tlen) {
		*p++ = '"';
		strncpy(p, (const char *) szTableName, tlen);
		*p += tlen;
		*p++ = '"';
		first = 0;
	}
	if (slen) {
		if (!first)
			*p++ = ',';
		*p++ = '"';
		strncpy(p, (const char *) szSchemaName, slen);
		*p += slen;
		*p++ = '"';
		first = 0;
	}
	if (clen) {
		if (!first)
			*p++ = ',';
		*p++ = '"';
		strncpy(p, (const char *) szCatalogName, clen);
		*p += clen;
		*p++ = '"';
		first = 0;
	}
	if (ttlen) {
		if (!first)
			*p++ = ',';
		*p++ = '"';
		strncpy(p, (const char *) szTableType, ttlen);
		*p += ttlen;
		*p++ = '"';
		first = 0;
	}
	*p = '\0';
	/* fprintf(stderr,"\nquery = %s\n",query); */

	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, query, p - query)) {
		free(query);
		return SQL_ERROR;
	}
	free(query);

	result = _SQLExecute(stmt);

	/* Sybase seem to return column name in lower case, 
	 * transform to uppercase 
	 * specification of ODBC and Perl test require these name be uppercase */
	odbc_upper_column_names(stmt->hdbc->tds_socket);

	return result;
}

static int
sql_to_c_type_default(int sql_type)
{

	switch (sql_type) {

	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
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
	default:
		return 0;
	}
}
