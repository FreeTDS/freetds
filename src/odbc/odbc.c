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

#ifdef UNIXODBC
    #include <sql.h>
    #include <sqlext.h>
    #include <odbcinst.h>
#else
    #include "isql.h"
    #include "isqlext.h"
#endif

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "tdsconvert.h"

#include "connectparams.h"
#include "odbc_util.h"
#include "convert_tds2sql.h"
#include "prepare_query.h"
#include "replacements.h"

static char  software_version[]   = "$Id: odbc.c,v 1.96 2002-11-27 15:51:06 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
    no_unused_var_warn};

static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR *phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR *phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR *phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);
static char *strncpy_null(char *dst, const char *src, int len);
static int sql_to_c_type_default ( int sql_type );
static int mymessagehandler(TDSCONTEXT* ctx, TDSSOCKET* tds, TDSMSGINFO* msg);
static int myerrorhandler(TDSCONTEXT* ctx, TDSSOCKET* tds, TDSMSGINFO* msg);


/* utils to check handles */
#define CHECK_HDBC  if ( SQL_NULL_HDBC  == hdbc  ) return SQL_INVALID_HANDLE;
#define CHECK_HSTMT if ( SQL_NULL_HSTMT == hstmt ) return SQL_INVALID_HANDLE;
#define CHECK_HENV  if ( SQL_NULL_HENV  == henv  ) return SQL_INVALID_HANDLE;


/*
 * Driver specific connectionn information
 */

typedef struct
{
    struct _hdbc        hdbc;
    /* we could put some vars in here but I can not think of any reason why at this point */

} ODBCConnection;

/*
**
** Note: I *HATE* hungarian notation, it has to be the most idiotic thing
** I've ever seen. So, you will note it is avoided other than in the function
** declarations. "Gee, let's make our code totally hard to read and they'll
** beg for GUI tools"
** Bah!
*/

static SQLRETURN change_database (SQLHDBC hdbc, SQLCHAR *database)
{
    SQLRETURN ret;
    TDSSOCKET *tds;
    int marker;
    struct _hdbc *dbc = (struct _hdbc *) hdbc;
    char *query;

    /* FIXME quote dbname if needed */
    tds = (TDSSOCKET *) dbc->tds_socket;
    query = (char *) malloc(strlen(database)+5);
    if (!query)
        return SQL_ERROR;
    sprintf(query,"use %s", database);
    ret = tds_submit_query(tds,query);
    free(query);
    if (ret != TDS_SUCCEED)
    {
        odbc_LogError ("Could not change Database");
        return SQL_ERROR;
    }

    do
    {
        marker=tds_get_byte(tds);
        tds_process_default_tokens(tds,marker);
    } while (marker!=TDS_DONE_TOKEN);

    return SQL_SUCCESS;
}

/* spinellia@acm.org : copied shamelessly from change_database */
static SQLRETURN change_autocommit (SQLHDBC hdbc, int state)
{
SQLRETURN ret;
TDSSOCKET *tds;
int marker;
char query[80];
struct _hdbc *dbc = (struct _hdbc *) hdbc;

	tds = (TDSSOCKET *) dbc->tds_socket;
	
	/* mssql: SET IMPLICIT_TRANSACTION ON
	 * sybase: SET CHAINED ON */

	/* implicit transactions are on if autocommit is off :-| */
	if (TDS_IS_MSSQL(tds))
		sprintf(query,"set implicit_transactions %s", (state?"off":"on"));
	else
		sprintf(query,"set chained %s", (state?"off":"on"));

	tdsdump_log(TDS_DBG_INFO1, "change_autocommit: executing %s\n", query);

	ret = tds_submit_query(tds,query);
	if (ret != TDS_SUCCEED) {
		odbc_LogError ("Could not change transaction status");
		return SQL_ERROR;
	}

	dbc->autocommit_state = state;
	/* FIXME should check result ? */
	do {
		marker=tds_get_byte(tds);
		tds_process_default_tokens(tds,marker);
	} while (marker!=TDS_DONE_TOKEN);

	return SQL_SUCCESS;
}

static SQLRETURN do_connect ( SQLHDBC hdbc, TDSCONNECTINFO *connect_info )
{
struct _hdbc *dbc = (struct _hdbc *) hdbc;
struct _henv *env = dbc->henv;

	dbc->tds_socket = tds_alloc_socket(env->tds_ctx, 512);
	if (!dbc->tds_socket)
		return SQL_ERROR;
	tds_set_parent(dbc->tds_socket, (void *) dbc);
	tds_fix_connect(connect_info);

	/* fix login type */
	if (!connect_info->try_domain_login) {
		if (strchr(connect_info->user_name,'\\')) {
			connect_info->try_domain_login = 1;
			connect_info->try_server_login = 0;
		}
	}
	if (!connect_info->try_domain_login && !connect_info->try_server_login)
		connect_info->try_server_login = 1;

	if (tds_connect(dbc->tds_socket, connect_info) == TDS_FAIL) {
		odbc_LogError ("tds_connect failed");
		return SQL_ERROR;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDriverConnect(
                                  SQLHDBC            hdbc,
                                  SQLHWND            hwnd,
                                  SQLCHAR FAR       *szConnStrIn,
                                  SQLSMALLINT        cbConnStrIn,
                                  SQLCHAR FAR       *szConnStrOut,
                                  SQLSMALLINT        cbConnStrOutMax,
                                  SQLSMALLINT FAR   *pcbConnStrOut,
                                  SQLUSMALLINT       fDriverCompletion)
{
SQLRETURN ret;
struct _hdbc *dbc = (struct _hdbc *) hdbc;
TDSCONNECTINFO *connect_info;

	CHECK_HDBC;

	
	odbc_LogError("");

	connect_info = tds_alloc_connect(dbc->henv->tds_ctx->locale);
	if ( !connect_info )
	{
		odbc_LogError( "Out of memory" );
		return SQL_ERROR;
	}

	tdoParseConnectString( szConnStrIn, connect_info );

	if ( tds_dstr_isempty(&connect_info->server_name) )
	{
		odbc_LogError( "Could not find Servername or server parameter" );
		return SQL_ERROR;
	}

	if ( tds_dstr_isempty(&connect_info->user_name) )
	{
		odbc_LogError( "Could not find UID parameter" );
		return SQL_ERROR;
	}

	if ( (ret = do_connect( hdbc, connect_info )) != SQL_SUCCESS )
	{
		return ret;
	}

	if ( !tds_dstr_isempty(&connect_info->database) )
	{
		return change_database( hdbc, connect_info->database );
	}

	/* use the default database */
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLBrowseConnect(
                                  SQLHDBC            hdbc,
                                  SQLCHAR FAR       *szConnStrIn,
                                  SQLSMALLINT        cbConnStrIn,
                                  SQLCHAR FAR       *szConnStrOut,
                                  SQLSMALLINT        cbConnStrOutMax,
                                  SQLSMALLINT FAR   *pcbConnStrOut)
{
    CHECK_HDBC;
    odbc_LogError ("SQLBrowseConnect: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLColumnPrivileges(
                                     SQLHSTMT           hstmt,
                                     SQLCHAR FAR       *szCatalogName,
                                     SQLSMALLINT        cbCatalogName,
                                     SQLCHAR FAR       *szSchemaName,
                                     SQLSMALLINT        cbSchemaName,
                                     SQLCHAR FAR       *szTableName,
                                     SQLSMALLINT        cbTableName,
                                     SQLCHAR FAR       *szColumnName,
                                     SQLSMALLINT        cbColumnName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLColumnPrivileges: function not implemented");
    /* FIXME: error HYC00, Driver not capable */
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLDescribeParam(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       ipar,
                                  SQLSMALLINT FAR   *pfSqlType,
                                  SQLUINTEGER FAR   *pcbParamDef,
                                  SQLSMALLINT FAR   *pibScale,
                                  SQLSMALLINT FAR   *pfNullable)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLDescribeParam: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLExtendedFetch(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       fFetchType,
                                  SQLINTEGER         irow,
                                  SQLUINTEGER FAR   *pcrow,
                                  SQLUSMALLINT FAR  *rgfRowStatus)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLExtendedFetch: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLForeignKeys(
                                SQLHSTMT           hstmt,
                                SQLCHAR FAR       *szPkCatalogName,
                                SQLSMALLINT        cbPkCatalogName,
                                SQLCHAR FAR       *szPkSchemaName,
                                SQLSMALLINT        cbPkSchemaName,
                                SQLCHAR FAR       *szPkTableName,
                                SQLSMALLINT        cbPkTableName,
                                SQLCHAR FAR       *szFkCatalogName,
                                SQLSMALLINT        cbFkCatalogName,
                                SQLCHAR FAR       *szFkSchemaName,
                                SQLSMALLINT        cbFkSchemaName,
                                SQLCHAR FAR       *szFkTableName,
                                SQLSMALLINT        cbFkTableName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLForeignKeys: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLMoreResults(
                                SQLHSTMT           hstmt)
{
    TDSSOCKET * tds;
    struct _hstmt *stmt;
    TDS_INT  result_type;

    CHECK_HSTMT;

    stmt=(struct _hstmt *)hstmt;
    tds = stmt->hdbc->tds_socket;

	/* try to go to the next recordset */
	for(;;) {
		switch (tds_process_result_tokens(tds, &result_type))
		{
		case TDS_NO_MORE_RESULTS:
			return SQL_NO_DATA_FOUND;
		case TDS_SUCCEED:
			switch(result_type) {
			case  TDS_COMPUTE_RESULT    :
			case  TDS_ROW_RESULT        :
			case  TDS_CMD_FAIL          :
				/* FIXME this row is used only as a flag for update binding, should be cleared if binding/result chenged */
				stmt->row = 0;
				return SQL_SUCCESS;

			case  TDS_STATUS_RESULT     :
				odbc_set_return_status(stmt);
				break;

			/* ?? */
			case  TDS_CMD_DONE          :
				if (tds->res_info) {
					stmt->row = 0;
					return SQL_SUCCESS;
				}

			case  TDS_PARAM_RESULT      :
			case  TDS_COMPUTEFMT_RESULT :
			case  TDS_MSG_RESULT        :
			case  TDS_ROWFMT_RESULT     :
			case  TDS_DESCRIBE_RESULT   :
				break;
			}
		}
	}
	return SQL_ERROR;
}

SQLRETURN SQL_API SQLNativeSql(
                              SQLHDBC            hdbc,
                              SQLCHAR FAR       *szSqlStrIn,
                              SQLINTEGER         cbSqlStrIn,
                              SQLCHAR FAR       *szSqlStr,
                              SQLINTEGER         cbSqlStrMax,
                              SQLINTEGER FAR    *pcbSqlStr)
{
    CHECK_HDBC;
    odbc_LogError ("SQLNativeSql: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLNumParams(
                              SQLHSTMT           hstmt,
                              SQLSMALLINT FAR   *pcpar)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;
    *pcpar = stmt->param_count;
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamOptions(
                                 SQLHSTMT           hstmt,
                                 SQLUINTEGER        crow,
                                 SQLUINTEGER FAR   *pirow)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLParamOptions: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLPrimaryKeys(
                                SQLHSTMT           hstmt,
                                SQLCHAR FAR       *szCatalogName,
                                SQLSMALLINT        cbCatalogName,
                                SQLCHAR FAR       *szSchemaName,
                                SQLSMALLINT        cbSchemaName,
                                SQLCHAR FAR       *szTableName,
                                SQLSMALLINT        cbTableName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLPrimaryKeys: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLProcedureColumns(
                                     SQLHSTMT           hstmt,
                                     SQLCHAR FAR       *szCatalogName,
                                     SQLSMALLINT        cbCatalogName,
                                     SQLCHAR FAR       *szSchemaName,
                                     SQLSMALLINT        cbSchemaName,
                                     SQLCHAR FAR       *szProcName,
                                     SQLSMALLINT        cbProcName,
                                     SQLCHAR FAR       *szColumnName,
                                     SQLSMALLINT        cbColumnName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLProcedureColumns: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLProcedures(
                               SQLHSTMT           hstmt,
                               SQLCHAR FAR       *szCatalogName,
                               SQLSMALLINT        cbCatalogName,
                               SQLCHAR FAR       *szSchemaName,
                               SQLSMALLINT        cbSchemaName,
                               SQLCHAR FAR       *szProcName,
                               SQLSMALLINT        cbProcName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLProcedures: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLSetPos(
                           SQLHSTMT           hstmt,
                           SQLUSMALLINT       irow,
                           SQLUSMALLINT       fOption,
                           SQLUSMALLINT       fLock)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLSetPos: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLTablePrivileges(
                                    SQLHSTMT           hstmt,
                                    SQLCHAR FAR       *szCatalogName,
                                    SQLSMALLINT        cbCatalogName,
                                    SQLCHAR FAR       *szSchemaName,
                                    SQLSMALLINT        cbSchemaName,
                                    SQLCHAR FAR       *szTableName,
                                    SQLSMALLINT        cbTableName)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLTablePrivileges: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLSetEnvAttr (
                                SQLHENV henv,
                                SQLINTEGER Attribute,
                                SQLPOINTER Value,
                                SQLINTEGER StringLength)
{
    CHECK_HENV;
    odbc_LogError ("SQLSetEnvAttr: function not implemented");
    return SQL_ERROR;
}


SQLRETURN SQL_API SQLBindParameter(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       ipar,
                                  SQLSMALLINT        fParamType,
                                  SQLSMALLINT        fCType,
                                  SQLSMALLINT        fSqlType,
                                  SQLUINTEGER        cbColDef,
                                  SQLSMALLINT        ibScale,
                                  SQLPOINTER         rgbValue,
                                  SQLINTEGER         cbValueMax,
                                  SQLINTEGER FAR    *pcbValue)
{
    struct _hstmt *stmt;
    struct _sql_param_info *cur, *newitem;

    CHECK_HSTMT;
    if (ipar == 0)
        return SQL_ERROR;

    stmt = (struct _hstmt *) hstmt;

    /* find available item in list */
    cur = odbc_find_param(stmt, ipar);

    if (!cur)
    {
        /* didn't find it create a new one */
        newitem = (struct _sql_param_info *) 
                  malloc(sizeof(struct _sql_param_info));
        if (!newitem)
            return SQL_ERROR;
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
		if (cur->param_bindtype==0)
			return SQL_ERROR;
	} else {
		cur->param_bindtype = fCType;
	}
	cur->param_sqltype = fSqlType;
	if (cur->param_bindtype==SQL_C_CHAR)
		cur->param_bindlen = cbValueMax;
	cur->param_lenbind = (char *) pcbValue;
	cur->varaddr = (char *) rgbValue;

	return SQL_SUCCESS;
}

#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API SQLAllocHandle(
                                SQLSMALLINT HandleType,
                                SQLHANDLE InputHandle,
                                SQLHANDLE * OutputHandle)
{
    switch (HandleType)
    {
    case SQL_HANDLE_STMT:
        return _SQLAllocStmt(InputHandle,OutputHandle);
        break;
    case SQL_HANDLE_DBC:
        return _SQLAllocConnect(InputHandle,OutputHandle);
        break;
    case SQL_HANDLE_ENV:
        return _SQLAllocEnv(OutputHandle);
        break;
    }
    return SQL_ERROR;
}
#endif

static SQLRETURN SQL_API _SQLAllocConnect(
                                         SQLHENV            henv,
                                         SQLHDBC FAR       *phdbc)
{
    struct _henv *env;
    ODBCConnection* dbc;

    CHECK_HENV;

    env = (struct _henv *) henv;
    dbc = (ODBCConnection*) malloc (sizeof (ODBCConnection));
    if (!dbc)
        return SQL_ERROR;

    memset(dbc,'\0',sizeof (ODBCConnection));
    dbc->hdbc.henv=env;
    dbc->hdbc.tds_login= (void *) tds_alloc_login();
    *phdbc = (SQLHDBC)dbc;
    /* spinellia@acm.org
     * after login is enabled autocommit */
    dbc->hdbc.autocommit_state = 1;

    return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocConnect(
                                 SQLHENV            henv,
                                 SQLHDBC FAR       *phdbc)
{
    return _SQLAllocConnect(henv, phdbc);
}

static SQLRETURN SQL_API _SQLAllocEnv(
                                     SQLHENV FAR       *phenv)
{
struct _henv *env;
TDSCONTEXT* ctx;

	env = (struct _henv*) malloc(sizeof(struct _henv));
	if (!env)
		return SQL_ERROR;

	memset(env,'\0',sizeof(struct _henv));
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
	free(ctx->locale->date_fmt);
	ctx->locale->date_fmt = strdup("%Y-%m-%d");

	*phenv = (SQLHENV)env;

	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocEnv(
                             SQLHENV FAR       *phenv)
{
    return _SQLAllocEnv(phenv);
}

static SQLRETURN SQL_API _SQLAllocStmt(
                                      SQLHDBC            hdbc,
                                      SQLHSTMT FAR      *phstmt)
{
    struct _hdbc *dbc;
    struct _hstmt *stmt;

    CHECK_HDBC;

    dbc = (struct _hdbc *) hdbc;

    stmt = (struct _hstmt*) malloc(sizeof(struct _hstmt));
    if (!stmt)
        return SQL_ERROR;
    memset(stmt,'\0',sizeof(struct _hstmt));
    stmt->hdbc = dbc;
    *phstmt = (SQLHSTMT)stmt;

    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocStmt(
                              SQLHDBC            hdbc,
                              SQLHSTMT FAR      *phstmt)
{
    return _SQLAllocStmt(hdbc,phstmt);
}

SQLRETURN SQL_API 
SQLBindCol(
          SQLHSTMT           hstmt,
          SQLUSMALLINT       icol,
          SQLSMALLINT        fCType,
          SQLPOINTER         rgbValue,
          SQLINTEGER         cbValueMax,
          SQLINTEGER FAR    *pcbValue)
{
    struct _hstmt *stmt;
    struct _sql_bind_info *cur, *prev = NULL, *newitem;

    CHECK_HSTMT;
    if (icol == 0)
        return SQL_ERROR;

    stmt = (struct _hstmt *) hstmt;

    /* find available item in list */
    cur = stmt->bind_head;
    while (cur)
    {
        if (cur->column_number==icol)
            break;
        prev = cur;
        cur = cur->next;
    }

    if (!cur)
    {
        /* didn't find it create a new one */
        newitem = (struct _sql_bind_info *) malloc(sizeof(struct _sql_bind_info));
        if (!newitem)
            return SQL_ERROR;
        memset(newitem, 0, sizeof(struct _sql_bind_info));
        newitem->column_number = icol;
        /* if there's no head yet */
        if (!stmt->bind_head)
        {
            stmt->bind_head = newitem;
        }
        else
        {
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

SQLRETURN SQL_API SQLCancel(
                           SQLHSTMT           hstmt)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;
    TDSSOCKET *tds = (TDSSOCKET *) stmt->hdbc->tds_socket;

    CHECK_HSTMT;

    tds_send_cancel(tds);
    tds_process_cancel(tds);

    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLConnect(
                            SQLHDBC            hdbc,
                            SQLCHAR FAR       *szDSN,
                            SQLSMALLINT        cbDSN,
                            SQLCHAR FAR       *szUID,
                            SQLSMALLINT        cbUID,
                            SQLCHAR FAR       *szAuthStr,
                            SQLSMALLINT        cbAuthStr)
{
const char *DSN;
SQLRETURN   nRetVal;
struct _hdbc *dbc = (struct _hdbc *) hdbc;
TDSCONNECTINFO *connect_info;

	CHECK_HDBC;

	odbc_LogError("");

	connect_info = tds_alloc_connect(dbc->henv->tds_ctx->locale);
	if ( !connect_info )
	{
		odbc_LogError( "Out of memory" );
		return SQL_ERROR;
	}

	/* data source name */
	if ( szDSN || (*szDSN) )
		DSN = szDSN;
	else
		DSN = "DEFAULT";

	if ( !odbc_get_dsn_info(DSN, connect_info) ) {
		odbc_LogError( "Error getting DSN information" );
		return SQL_ERROR;
	}

	/* username/password are never saved to ini file, 
	 * so you do not check in ini file */
	/* user id */
	if ( szUID && (*szUID) )
		tds_dstr_copy(&connect_info->user_name,szUID);

	/* password */
	if ( szAuthStr )
		tds_dstr_copy(&connect_info->password,szAuthStr);

	/* DO IT */
	if ( (nRetVal = do_connect( hdbc, connect_info)) != SQL_SUCCESS )
		return nRetVal;

	/* database */
	if ( !tds_dstr_isempty(&connect_info->database) )
		return change_database( hdbc, connect_info->database );

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeCol(
                                SQLHSTMT           hstmt,
                                SQLUSMALLINT       icol,
                                SQLCHAR FAR       *szColName,
                                SQLSMALLINT        cbColNameMax,
                                SQLSMALLINT FAR   *pcbColName,
                                SQLSMALLINT FAR   *pfSqlType,
                                SQLUINTEGER FAR   *pcbColDef,
                                SQLSMALLINT FAR   *pibScale,
                                SQLSMALLINT FAR   *pfNullable)
{
    TDSSOCKET *tds;
    TDSCOLINFO *colinfo;
    int cplen, namelen;
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;

    tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    if (icol == 0 || icol > tds->res_info->num_cols)
    {
        odbc_LogError ("SQLDescribeCol: Column out of range");
        return SQL_ERROR;
    }
    /* check name length */
    if (cbColNameMax < 0) {
	    /* HY090 */
	    odbc_LogError ("Invalid buffer length");
	    return SQL_ERROR;
    }
    colinfo = tds->res_info->columns[icol-1];

    /* cbColNameMax can be 0 (to retrieve name length) */
    if (szColName && cbColNameMax)
    {
	/* straight copy column name up to cbColNameMax */
        namelen = strlen(colinfo->column_name);
        cplen = namelen >= cbColNameMax ?
                cbColNameMax - 1 : namelen;
        strncpy(szColName,colinfo->column_name, cplen);
        szColName[cplen]='\0';
        /* fprintf(stderr,"\nsetting column name to %s %s\n", colinfo->column_name, szColName); */
    }
    if (pcbColName)
    {
	/* return column name length (without terminator) 
	 * as specification return full length, not copied length */
        *pcbColName = strlen(colinfo->column_name);
    }
    if (pfSqlType)
    {
        *pfSqlType=odbc_get_client_type(colinfo->column_type, colinfo->column_size);
    }

    if (pcbColDef)
    {
        if (is_numeric_type(colinfo->column_type))
        {
            *pcbColDef = colinfo->column_prec;
        }
        else
        {
            *pcbColDef = colinfo->column_size;
        }
    }
    if (pibScale)
    {
        if (is_numeric_type(colinfo->column_type))
        {
            *pibScale = colinfo->column_scale;
        }
        else
        {
            *pibScale = 0;
        }
    }
    if (pfNullable)
    {
        *pfNullable = is_nullable_type(colinfo->column_type) ? 1 : 0;
    }
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColAttributes(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       icol,
                                  SQLUSMALLINT       fDescType,
                                  SQLPOINTER         rgbDesc,
                                  SQLSMALLINT        cbDescMax,
                                  SQLSMALLINT FAR   *pcbDesc,
                                  SQLINTEGER FAR    *pfDesc)
{
    TDSSOCKET *tds;
    TDSCOLINFO *colinfo;
    int cplen, len = 0;
    struct _hstmt *stmt;
    struct _hdbc *dbc;

    CHECK_HSTMT;

    stmt = (struct _hstmt *) hstmt;
    dbc = (struct _hdbc *) stmt->hdbc;
    tds = (TDSSOCKET *) dbc->tds_socket;


    /* dont check column index for these */
    switch (fDescType)
    {
    case SQL_COLUMN_COUNT:
        if (!tds->res_info)
        {
            *pfDesc = 0;
        }
        else
        {
            *pfDesc = tds->res_info->num_cols;
        }
        return SQL_SUCCESS;
        break;
    }

    if (!tds->res_info)
    {
        odbc_LogError ("SQLDescribeCol: Query Returned No Result Set!");
        return SQL_ERROR;
    }

    if (icol == 0 || icol > tds->res_info->num_cols)
    {
        odbc_LogError ("SQLDescribeCol: Column out of range");
        return SQL_ERROR;
    }
    colinfo = tds->res_info->columns[icol-1];

    tdsdump_log(TDS_DBG_INFO1, "odbc:SQLColAttributes: fDescType is %d\n", fDescType);
    switch (fDescType)
    {
    case SQL_COLUMN_NAME:
    case SQL_COLUMN_LABEL:
        len = strlen(colinfo->column_name);
        cplen = len > (cbDescMax-1) ? (cbDescMax-1) : len;
        tdsdump_log(TDS_DBG_INFO2, "SQLColAttributes: copying %d bytes, len = %d, cbDescMax = %d\n",cplen, len, cbDescMax);
        strncpy(rgbDesc,colinfo->column_name,cplen);
        ((char *)rgbDesc)[cplen]='\0';
	/* return length of full string, not only copied part */
        if (pcbDesc)
        {
            *pcbDesc = len;
        }
        break;
    case SQL_COLUMN_TYPE:
    case SQL_DESC_TYPE:
        *pfDesc=odbc_get_client_type(colinfo->column_type, colinfo->column_size);
        tdsdump_log(TDS_DBG_INFO2, "odbc:SQLColAttributes: colinfo->column_type = %d,"
                    " colinfo->column_size = %d,"
                    " *pfDesc = %d\n"
                    , colinfo->column_type, colinfo->column_size, *pfDesc);
        break;
    case SQL_COLUMN_PRECISION: /* this section may be wrong */
        switch (colinfo->column_type)
        {
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
        switch (odbc_get_client_type(colinfo->column_type, colinfo->column_size))
        {
        case SQL_CHAR:
        case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
            *pfDesc = colinfo->column_size;
            break;
	case SQL_BIGINT:
	    *pfDesc = 20;
	    break;
        case SQL_INTEGER:
            *pfDesc = 11; /* -1000000000 */
            break;
        case SQL_SMALLINT:
            *pfDesc = 6; /* -10000 */
            break;
        case SQL_TINYINT:
            *pfDesc = 3; /* 255 */
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
	    *pfDesc = 24; /* FIXME check, always format 
			     yyyy-mm-dd hh:mm:ss[.fff] ?? */
	    /* spinellia@acm.org: int token.c it is 30 should we comply? */
	    break;
        case SQL_FLOAT:
        case SQL_REAL:
        case SQL_DOUBLE:
            *pfDesc = 24; /* FIXME -- what should the correct size be? */
            break;
	case SQL_GUID:
	    *pfDesc = 36;
	    break;
	default:
	    /* FIXME TODO finish, should support ALL types (interval) */
	    *pfDesc = 40;
	    tdsdump_log( TDS_DBG_INFO1,
			    "SQLColAttributes(%d,SQL_COLUMN_DISPLAY_SIZE): unknown client type %d\n",
			    icol, 
			    odbc_get_client_type(colinfo->column_type, colinfo->column_size)
			    );
	    break;
        }
        break;
	/* FIXME other types ...*/
	default:
		tdsdump_log(TDS_DBG_INFO2, "odbc:SQLColAttributes: fDescType %d not catered for...\n");
		break;
    }
    return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLDisconnect(
                               SQLHDBC            hdbc)
{
    struct _hdbc *dbc;

    CHECK_HDBC;

    dbc = (struct _hdbc *) hdbc;
    tds_free_socket(dbc->tds_socket);

    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLError(
                          SQLHENV            henv,
                          SQLHDBC            hdbc,
                          SQLHSTMT           hstmt,
                          SQLCHAR FAR       *szSqlState,
                          SQLINTEGER FAR    *pfNativeError,
                          SQLCHAR FAR       *szErrorMsg,
                          SQLSMALLINT        cbErrorMsgMax,
                          SQLSMALLINT FAR   *pcbErrorMsg)
{
    SQLRETURN result = SQL_NO_DATA_FOUND;

    if (strlen (odbc_GetLastError()) > 0)
    {
	/* change all error handling, error should be different.. */
        strcpy (szSqlState, "08001");
        strcpy (szErrorMsg, odbc_GetLastError());
        if (pcbErrorMsg)
            *pcbErrorMsg = strlen (odbc_GetLastError());
        if (pfNativeError)
            *pfNativeError = 1;

        result = SQL_SUCCESS;
        odbc_LogError ("");
    }

    return result;
}

static int mymessagehandler( 
	TDSCONTEXT* ctx,
	TDSSOCKET* tds,
	TDSMSGINFO* msg
)
{
char *p;

	if (asprintf( &p,
		" Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
		msg->msg_number,
		msg->msg_level,
		msg->msg_state,
		msg->server,
		msg->line_number,
		msg->message
	) < 0) return 0;
	/* latest_msg_number = msg->msg_number; */
	odbc_LogError( p );
	free(p);
	return 1;
}

static int myerrorhandler( 
	TDSCONTEXT* ctx,
	TDSSOCKET* tds,
	TDSMSGINFO* msg
)
{
char *p;

	if (asprintf( &p,
		" Err %d, Level %d, State %d, Server %s, Line %d\n%s\n",
		msg->msg_number,
		msg->msg_level,
		msg->msg_state,
		msg->server,
		msg->line_number,
		msg->message
	) < 0) return 0;
	odbc_LogError( p );
	free(p);
	return 1;
}

static SQLRETURN SQL_API 
_SQLExecute( SQLHSTMT hstmt)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;
    int ret;
    TDSSOCKET *tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    TDS_INT    result_type;
    TDS_INT    done = 0;
	SQLRETURN result = SQL_SUCCESS;

    CHECK_HSTMT;

    stmt->row = 0;

    if (!(tds_submit_query(tds, stmt->query)==TDS_SUCCEED))
    {
/*        odbc_LogError (tds->msg_info->message); */
        return SQL_ERROR;
    }
    stmt->hdbc->current_statement = stmt;

    /* TODO review this, ODBC return parameter in other way, for compute I don't know */
    while ((ret = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED)
    {
      switch (result_type) {
        case  TDS_COMPUTE_RESULT    :
        case  TDS_PARAM_RESULT      :
        case  TDS_ROW_RESULT        :
        case  TDS_STATUS_RESULT     :
			done = 1;
			break;
		case  TDS_CMD_FAIL          :
			result = SQL_ERROR;
              done = 1;
              break;

        case  TDS_CMD_DONE          :
			if (tds->res_info) done = 1;
        case  TDS_COMPUTEFMT_RESULT :
        case  TDS_MSG_RESULT        :
        case  TDS_ROWFMT_RESULT     :
        case  TDS_DESCRIBE_RESULT   :
              break;

      }
      if (done) break;
    }
    if (ret==TDS_NO_MORE_RESULTS) {
        return result;
    } else if (ret==TDS_SUCCEED) {
        return result;
    } else {
	tdsdump_log(TDS_DBG_INFO1, "SQLExecute: bad results\n" );
        return result;
    }
}

SQLRETURN SQL_API SQLExecDirect(
                               SQLHSTMT           hstmt,
                               SQLCHAR FAR       *szSqlStr,
                               SQLINTEGER         cbSqlStr)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;

    stmt->param_count = 0;
    if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, (char*)szSqlStr, cbSqlStr))
        return SQL_ERROR;
    if (SQL_SUCCESS!=prepare_call(stmt))
        return SQL_ERROR;

    return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLExecute(
                            SQLHSTMT           hstmt)
{
#ifdef ENABLE_DEVELOPING
TDSSOCKET *tds;
TDSDYNAMIC *dyn;
int marker;
struct _sql_param_info *param;
TDS_INT  result_type;
int ret, done;
SQLRETURN result = SQL_SUCCESS;
#endif 
struct _hstmt *stmt = (struct _hstmt *) hstmt;

	CHECK_HSTMT;

	/* translate to native format */
	if (SQL_SUCCESS!=prepare_call(stmt))
		return SQL_ERROR;

#ifdef ENABLE_DEVELOPING
	tds = stmt->hdbc->tds_socket;

	if (stmt->param_count > 0) {
		/* prepare dynamic query (only for first SQLExecute call) */
		if (!stmt->dyn) {
			tdsdump_log(TDS_DBG_INFO1,"Creating prepared statement\n");
			if (tds_submit_prepare(tds, stmt->prepared_query, NULL, &stmt->dyn) == TDS_FAIL)
				return SQL_ERROR;
			/* TODO get results and other things */
			do
			{
				marker=tds_get_byte(tds);
				tds_process_default_tokens(tds,marker);
			/* FIXME is DEAD loop do not end...Put all in tds_submit_prepare ?? */
			} while (tds->state != TDS_COMPLETED);
		}
		/* build parameters list */
		dyn = stmt->dyn;
		/* TODO rebuild should be done for every bingings change */
		/*if (dyn->num_params != stmt->param_count) */ {
			int i;
			TDSPARAMINFO *params;
			TDSCOLINFO *curcol;
			tds_free_input_params(dyn);
			tdsdump_log(TDS_DBG_INFO1,"Setting input parameters\n");
			for(i=0; i < stmt->param_count; ++i) {
				param = odbc_find_param(stmt, i+1);
				if (!param) return SQL_ERROR;
				if (!(params=tds_alloc_param_result(dyn->params)))
					return SQL_ERROR;
				dyn->params = params;
				/* add another type and copy data */
				curcol = params->columns[i];
				/* TODO handle bindings of char like "{d '2002-11-12'}" */
				/* FIXME run only with VARCHAR */
				tds_set_column_type(curcol, SYBVARCHAR);
				curcol->column_cur_size = curcol->column_size = *(SQLINTEGER*)param->param_lenbind;
				tds_alloc_param_row(params, curcol);
				memcpy(&params->current_row[curcol->column_offset], param->varaddr, *(SQLINTEGER*)param->param_lenbind);
			}
		}
		tdsdump_log(TDS_DBG_INFO1,"End prepare, execute\n");
		/* TODO check errors */
		if (tds_submit_execute(tds, dyn) == TDS_FAIL)
			return SQL_ERROR;
		
		/* TODO copied from _SQLExecute, use a function... */
		stmt->hdbc->current_statement = stmt;

		done = 0;
		while ((ret = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED)
		{
			switch (result_type) {
			case  TDS_COMPUTE_RESULT    :
			case  TDS_ROW_RESULT        :
				done = 1;
				break;
			case  TDS_CMD_FAIL          :
				result = SQL_ERROR;
				done = 1;
				break;

			case  TDS_CMD_DONE          :
				done = 1;
				break;

			case  TDS_PARAM_RESULT      :
			case  TDS_STATUS_RESULT     :
			case  TDS_COMPUTEFMT_RESULT :
			case  TDS_MSG_RESULT        :
			case  TDS_ROWFMT_RESULT     :
			case  TDS_DESCRIBE_RESULT   :
				break;

			}
			if (done) break;
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

	if (stmt->prepared_query)
	{
		SQLRETURN res = start_parse_prepared_query(stmt);
		if (SQL_SUCCESS!=res)
			return res;
	}

	return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLFetch(
                          SQLHSTMT           hstmt)
{
    int ret;
    TDSSOCKET *tds;
    TDSRESULTINFO * resinfo;
    TDSCOLINFO *colinfo;
    int i;
    struct _hstmt *stmt;
    SQLINTEGER len=0;
    TDS_CHAR *src;
    int srclen;
    struct _sql_bind_info *cur;
    TDSLOCINFO *locale;
    TDSCONTEXT *context;
    TDS_INT    rowtype;
    TDS_INT    computeid;


    CHECK_HSTMT;

    stmt=(struct _hstmt *)hstmt;

    tds = stmt->hdbc->tds_socket;

    context = stmt->hdbc->henv->tds_ctx;
    locale = context->locale;

    /* if we bound columns, transfer them to res_info now that we have one */
    if (stmt->row==0)
    {
        cur = stmt->bind_head;
        while (cur)
        {
            if (cur->column_number>0 && 
                cur->column_number <= tds->res_info->num_cols)
            {
                colinfo = tds->res_info->columns[cur->column_number-1];
                colinfo->column_varaddr = cur->varaddr;
                colinfo->column_bindtype = cur->column_bindtype;
                colinfo->column_bindlen = cur->column_bindlen;
                colinfo->column_lenbind = cur->column_lenbind;
            }
            else
            {
                /* log error ? */
            }
            cur = cur->next;
        }
    }
    stmt->row++;

    ret = tds_process_row_tokens(stmt->hdbc->tds_socket, &rowtype, &computeid);
    if (ret==TDS_NO_MORE_ROWS) {
	tdsdump_log(TDS_DBG_INFO1, "SQLFetch: NO_DATA_FOUND\n" );
        return SQL_NO_DATA_FOUND;
    }
    resinfo = tds->res_info;
    if (!resinfo) {
	tdsdump_log(TDS_DBG_INFO1, "SQLFetch: !resinfo\n" );
        return SQL_NO_DATA_FOUND;
    }
    for (i=0;i<resinfo->num_cols;i++)
    {
        colinfo = resinfo->columns[i];
        colinfo->column_text_sqlgetdatapos = 0;
        if (colinfo->column_varaddr && !tds_get_null(resinfo->current_row, i))
        {
			src = (TDS_CHAR*)&resinfo->current_row[colinfo->column_offset];
			if (is_blob_type(colinfo->column_type))
				src = ((TDSBLOBINFO *) src)->textvalue;
            srclen = colinfo->column_cur_size;
            len = convert_tds2sql(context, 
                                  tds_get_conversion_type(colinfo->column_type, colinfo->column_size),
                                  src,
                                  srclen, 
                                  colinfo->column_bindtype, 
                                  colinfo->column_varaddr, 
                                  colinfo->column_bindlen);
        }
        if (colinfo->column_lenbind)
        {
        	if (tds_get_null(resinfo->current_row, i))
            	*((SQLINTEGER *)colinfo->column_lenbind)=SQL_NULL_DATA;
			else
            	*((SQLINTEGER *)colinfo->column_lenbind)=len;
        }
    }
    if (ret==TDS_SUCCEED) {
        return SQL_SUCCESS;
    } else {
	tdsdump_log(TDS_DBG_INFO1, "SQLFetch: !TDS_SUCCEED (%d)\n", ret );
        return SQL_ERROR;
    }
}


#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API SQLFreeHandle(
                               SQLSMALLINT HandleType,
                               SQLHANDLE Handle)
{
    tdsdump_log(TDS_DBG_INFO1, "SQLFreeHandle(%d, 0x%x)\n", HandleType, Handle);

    switch (HandleType)
    {
    case SQL_HANDLE_STMT:
        return _SQLFreeStmt(Handle,SQL_DROP);
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

static SQLRETURN SQL_API _SQLFreeConnect(
                                        SQLHDBC            hdbc)
{
    ODBCConnection* dbc = (ODBCConnection*) hdbc;

    CHECK_HDBC;

    free (dbc);

    return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeConnect(
                                SQLHDBC            hdbc)
{
    return _SQLFreeConnect(hdbc);
}
#endif

static SQLRETURN SQL_API _SQLFreeEnv(
                                    SQLHENV            henv)
{
    CHECK_HENV;
    return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeEnv(
                            SQLHENV            henv)
{
    return _SQLFreeEnv(henv);
}

static SQLRETURN SQL_API 
_SQLFreeStmt(
            SQLHSTMT           hstmt,
            SQLUSMALLINT       fOption)
{
    TDSSOCKET * tds;
    struct _hstmt *stmt=(struct _hstmt *)hstmt;

    CHECK_HSTMT;

    /* check if option correct */
    if (fOption != SQL_DROP && fOption != SQL_CLOSE 
        && fOption != SQL_UNBIND && fOption != SQL_RESET_PARAMS)
    {
        tdsdump_log(TDS_DBG_ERROR, "odbc:SQLFreeStmt: Unknown option %d\n", fOption);
        /* FIXME: error HY092 */
        return SQL_ERROR;
    }

    /* if we have bound columns, free the temporary list */
    if (fOption == SQL_DROP || fOption == SQL_UNBIND)
    {
        struct _sql_bind_info *cur,*tmp;
        if (stmt->bind_head)
        {
            cur = stmt->bind_head;
            while (cur)
            {
                tmp = cur->next;
                free(cur);
                cur = tmp;
            }
            stmt->bind_head = NULL;
        }
    }

    /* do the same for bound parameters */
    if (fOption == SQL_DROP || fOption == SQL_RESET_PARAMS)
    {
        struct _sql_param_info *cur,*tmp;
        if (stmt->param_head)
        {
            cur = stmt->param_head;
            while (cur)
            {
                tmp = cur->next;
                free(cur);
                cur = tmp;
            }
            stmt->param_head = NULL;
        }
    }

    /* close statement */
    if (fOption == SQL_DROP || fOption == SQL_CLOSE )
    {
        tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
        /* 
        ** FIX ME -- otherwise make sure the current statement is complete
        */
	/* do not close other running query ! */
        if (tds->state==TDS_PENDING && stmt->hdbc->current_statement == stmt)
        {
            tds_send_cancel(tds);
            tds_process_cancel(tds);
        }
    }

    /* free it */
    if (fOption == SQL_DROP)
    {
        if (stmt->query)
            free(stmt->query);
        if (stmt->prepared_query)
            free(stmt->prepared_query);
        free(stmt);
    }
    return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeStmt(
                             SQLHSTMT           hstmt,
                             SQLUSMALLINT       fOption)
{
    return _SQLFreeStmt(hstmt, fOption);
}

SQLRETURN SQL_API SQLGetStmtAttr (
                                 SQLHSTMT hstmt,
                                 SQLINTEGER Attribute,
                                 SQLPOINTER Value,
                                 SQLINTEGER BufferLength,
                                 SQLINTEGER * StringLength)
{
	CHECK_HSTMT;

	if ( BufferLength == SQL_IS_UINTEGER ) {
		return SQLGetStmtOption(hstmt, Attribute, Value);
	} else {
		return SQL_ERROR;
	}
}

SQLRETURN SQL_API SQLGetCursorName(
                                  SQLHSTMT           hstmt,
                                  SQLCHAR FAR       *szCursor,
                                  SQLSMALLINT        cbCursorMax,
                                  SQLSMALLINT FAR   *pcbCursor)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLGetCursorName: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLNumResultCols(
                                  SQLHSTMT           hstmt,
                                  SQLSMALLINT FAR   *pccol)
{
    TDSRESULTINFO * resinfo;
    TDSSOCKET * tds;
    struct _hstmt *stmt;

    CHECK_HSTMT;

    stmt=(struct _hstmt *)hstmt;
    tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    resinfo = tds->res_info;
    if (resinfo == NULL)
    {
        /* 3/15/2001 bsb - DBD::ODBC calls SQLNumResultCols on non-result
        ** generating queries such as 'drop table' */
        *pccol = 0;
        return SQL_SUCCESS;
/*
       if (tds && tds->msg_info && tds->msg_info->message)
          odbc_LogError (tds->msg_info->message);
           else
          odbc_LogError ("SQLNumResultCols: resinfo is NULL");

       return SQL_ERROR;
*/
    }

    *pccol= resinfo->num_cols;
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrepare(
                            SQLHSTMT           hstmt,
                            SQLCHAR FAR       *szSqlStr,
                            SQLINTEGER         cbSqlStr)
{
	struct _hstmt *stmt=(struct _hstmt *)hstmt;

    CHECK_HSTMT;

    if (SQL_SUCCESS!=odbc_set_stmt_prepared_query(stmt, (char*)szSqlStr, cbSqlStr))
        return SQL_ERROR;

	/* count parameters */
	stmt->param_count = tds_count_placeholders(stmt->prepared_query);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLRowCount(
                             SQLHSTMT           hstmt,
                             SQLINTEGER FAR    *pcrow)
{
    TDSRESULTINFO * resinfo;
    TDSSOCKET * tds;
    struct _hstmt *stmt;

    CHECK_HSTMT;

/* 7/28/2001 begin l@poliris.com */
    stmt=(struct _hstmt *)hstmt;
    tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    resinfo = tds->res_info;
    if (resinfo == NULL)
    {
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
    *pcrow= resinfo->row_count;

/* end l@poliris.com */

    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetCursorName(
                                  SQLHSTMT           hstmt,
                                  SQLCHAR FAR       *szCursor,
                                  SQLSMALLINT        cbCursor)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLSetCursorName: function not implemented");
    return SQL_ERROR;
}


/* TODO join all this similar function... */
/* spinellia@acm.org : copied shamelessly from change_database */
/* transaction support */
/* 1 = commit, 0 = rollback */
static SQLRETURN change_transaction (SQLHDBC hdbc, int state)
{
SQLRETURN ret;
TDSSOCKET *tds;
int marker;
char query[256];
struct _hdbc *dbc = (struct _hdbc *) hdbc;
SQLRETURN cc = SQL_SUCCESS;

	tdsdump_log(TDS_DBG_INFO1, "change_transaction(0x%x,%d)\n",
		       hdbc, state );

	tds = (TDSSOCKET *) dbc->tds_socket;
	strcpy( query, ( state ? "commit" : "rollback" ));
	ret = tds_submit_query(tds,query);
	if (ret != TDS_SUCCEED) {
		odbc_LogError ("Could not perform COMMIT or ROLLBACK");
		cc = SQL_ERROR;
	}

	do {
		marker=tds_get_byte(tds);
		tds_process_default_tokens(tds,marker);
	} while (marker!=TDS_DONE_TOKEN);

	return cc;
}

SQLRETURN SQL_API SQLTransact(
                             SQLHENV            henv,
                             SQLHDBC            hdbc,
                             SQLUSMALLINT       fType)
{
int op = ( fType == SQL_COMMIT ? 1 : 0 );

	/* I may live without a HENV */
	/*     CHECK_HENV; */
	/* ..but not without a HDBC! */
	CHECK_HDBC;

	tdsdump_log(TDS_DBG_INFO1, "SQLTransact(0x%x,0x%x,%d)\n",
			henv, hdbc, fType );
	return change_transaction( hdbc, op );
}
/* end of transaction support */


SQLRETURN SQL_API SQLSetParam(            /*      Use SQLBindParameter */
                                          SQLHSTMT           hstmt,
                                          SQLUSMALLINT       ipar,
                                          SQLSMALLINT        fCType,
                                          SQLSMALLINT        fSqlType,
                                          SQLUINTEGER        cbParamDef,
                                          SQLSMALLINT        ibScale,
                                          SQLPOINTER         rgbValue,
                                          SQLINTEGER FAR     *pcbValue)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLSetParam: function not implemented");
    return SQL_ERROR;
}

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
SQLRETURN SQL_API SQLColumns(
                            SQLHSTMT           hstmt,
                            SQLCHAR FAR       *szCatalogName,   /* object_qualifier */
                            SQLSMALLINT        cbCatalogName, 
                            SQLCHAR FAR       *szSchemaName,    /* object_owner */
                            SQLSMALLINT        cbSchemaName,
                            SQLCHAR FAR       *szTableName,     /* object_name */
                            SQLSMALLINT        cbTableName,
                            SQLCHAR FAR       *szColumnName,    /* column_name */
                            SQLSMALLINT        cbColumnName )
{
    struct  _hstmt *stmt;
    char    szQuery[4096];
    int     nTableName      = odbc_get_string_size( cbTableName, szTableName );
    int     nTableOwner     = odbc_get_string_size( cbSchemaName, szSchemaName ); 
    int     nTableQualifier = odbc_get_string_size( cbCatalogName, szCatalogName ); 
    int     nColumnName     = odbc_get_string_size( cbColumnName, szColumnName );
    int     bNeedComma      = 0;

    CHECK_HSTMT;

    stmt = (struct _hstmt *) hstmt;

    sprintf( szQuery, "exec sp_columns " ); 

    if ( szTableName && *szTableName )
    {
        strcat( szQuery, "@table_name = '" );
        strncat( szQuery, szTableName, nTableName );
        strcat( szQuery, "'" );
        bNeedComma = 1;
    }

    if ( szSchemaName && *szSchemaName )
    {
        if ( bNeedComma )
            strcat( szQuery, ", " );
        strcat( szQuery, "@table_owner = '" );
        strncat( szQuery, szSchemaName, nTableOwner );
        strcat( szQuery, "'" );
        bNeedComma = 1;
    }

    if ( szCatalogName && *szCatalogName )
    {
        if ( bNeedComma )
            strcat( szQuery, ", " );
        strcat( szQuery, "@table_qualifier = '" );
        strncat( szQuery, szCatalogName, nTableQualifier );
        strcat( szQuery, "'" );
        bNeedComma = 1;
    }

    if ( szColumnName && *szColumnName )
    {
        if ( bNeedComma )
            strcat( szQuery, ", " );
        strcat( szQuery, "@column_name = '" );
        strncat( szQuery, szColumnName, nColumnName );
        strcat( szQuery, "'" );
        bNeedComma = 1;
    }

    if ( SQL_SUCCESS != odbc_set_stmt_query( stmt, szQuery, strlen( szQuery ) ) )
        return SQL_ERROR;

    return _SQLExecute( hstmt );
}

SQLRETURN SQL_API SQLGetConnectOption(
                                     SQLHDBC            hdbc,
                                     SQLUSMALLINT       fOption,
                                     SQLPOINTER         pvParam)
{
    struct _hdbc *dbc = (struct _hdbc *) hdbc;

    /* TODO implement more options
     * AUTOCOMMIT required by DBD::ODBC
     */
    CHECK_HDBC;
    switch (fOption)
    {
	case SQL_AUTOCOMMIT:
		* ( (SQLUINTEGER*) pvParam ) = dbc->autocommit_state;
		return SQL_SUCCESS;
	case SQL_TXN_ISOLATION:
		* ( (SQLUINTEGER*) pvParam ) = SQL_TXN_READ_COMMITTED;
		return SQL_SUCCESS;
	default:
        tdsdump_log(TDS_DBG_INFO1, "odbc:SQLGetConnectOption: Statement option %d not implemented\n", fOption);
        odbc_LogError ("Statement option not implemented");
        return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetData(
                            SQLHSTMT           hstmt,
                            SQLUSMALLINT       icol,
                            SQLSMALLINT        fCType,
                            SQLPOINTER         rgbValue,
                            SQLINTEGER         cbValueMax,
                            SQLINTEGER FAR    *pcbValue)
{
    TDSCOLINFO * colinfo;
    TDSRESULTINFO * resinfo;
    TDSSOCKET * tds;
    struct _hstmt *stmt;
    TDS_CHAR *src;
    int srclen;
    TDSLOCINFO *locale;
    TDSCONTEXT *context;
    SQLINTEGER dummy_cb;
    int nSybType;

    CHECK_HSTMT;

    if (!pcbValue) pcbValue = &dummy_cb;

    stmt = (struct _hstmt *) hstmt;
    tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    context = stmt->hdbc->henv->tds_ctx;
    locale = context->locale;
    resinfo = tds->res_info;
    if (icol == 0 || icol > tds->res_info->num_cols)
    {
        odbc_LogError ("SQLGetData: Column out of range");
        return SQL_ERROR;
    }
    colinfo = resinfo->columns[icol-1];

    if (tds_get_null(resinfo->current_row, icol-1))
    {
        *pcbValue=SQL_NULL_DATA;
    }
    else
    {
		src = (TDS_CHAR*)&resinfo->current_row[colinfo->column_offset];
        if (is_blob_type(colinfo->column_type))
        {
            if (colinfo->column_text_sqlgetdatapos >= colinfo->column_cur_size)
                return SQL_NO_DATA_FOUND;
            src = ((TDSBLOBINFO*) src)->textvalue + colinfo->column_text_sqlgetdatapos;
            srclen = colinfo->column_cur_size - colinfo->column_text_sqlgetdatapos;
        }
        else
        {
            srclen = colinfo->column_cur_size;
        }
        nSybType = tds_get_conversion_type( colinfo->column_type, colinfo->column_size );
        *pcbValue=convert_tds2sql(context, 
                                  nSybType,
                                  src,
                                  srclen, 
                                  fCType, 
                                  rgbValue,
                                  cbValueMax);

        if (is_blob_type(colinfo->column_type))
        {
	    /* calc how many bytes was readed */
	    int readed = cbValueMax;
	    /* char is always terminated...*/
	    /* FIXME test on destination char ??? */
	    if (nSybType == SYBTEXT) --readed;
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

static void _set_func_exists(SQLUSMALLINT FAR *pfExists, SQLUSMALLINT fFunction)
{
    SQLUSMALLINT FAR *mod;

    mod = pfExists + (fFunction >> 4);
    *mod |= (1 << (fFunction & 0x0f));
}

SQLRETURN SQL_API SQLGetFunctions(
                                 SQLHDBC            hdbc,
                                 SQLUSMALLINT       fFunction,
                                 SQLUSMALLINT FAR  *pfExists)
{

    CHECK_HDBC;

    tdsdump_log(TDS_DBG_FUNC, "SQLGetFunctions: fFunction is %d\n", fFunction);
    switch (fFunction)
    {
#if (ODBCVER >= 0x0300)
    case SQL_API_ODBC3_ALL_FUNCTIONS:
        /* This 1.0-2.0 ODBC driver, so driver managet will
           map ODBC 3 functions to ODBC 2 functions */
        return SQL_ERROR;
#endif

/*			for (i=0;i<SQL_API_ODBC3_ALL_FUNCTIONS_SIZE;i++) {
                pfExists[i] = 0xFFFF;
            }
*/
#if 0
        _set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLALLOCENV);
        _set_func_exists(pfExists,SQL_API_SQLALLOCHANDLE);
        _set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
        _set_func_exists(pfExists,SQL_API_SQLBINDCOL);
        _set_func_exists(pfExists,SQL_API_SQLBINDPARAMETER);
        _set_func_exists(pfExists,SQL_API_SQLCANCEL);
        _set_func_exists(pfExists,SQL_API_SQLCLOSECURSOR);
        _set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTE);
        _set_func_exists(pfExists,SQL_API_SQLCOLUMNS);
        _set_func_exists(pfExists,SQL_API_SQLCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLCOPYDESC);
        _set_func_exists(pfExists,SQL_API_SQLDATASOURCES);
        _set_func_exists(pfExists,SQL_API_SQLDESCRIBECOL);
        _set_func_exists(pfExists,SQL_API_SQLDISCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLENDTRAN);
        _set_func_exists(pfExists,SQL_API_SQLERROR);
        _set_func_exists(pfExists,SQL_API_SQLEXECDIRECT);
        _set_func_exists(pfExists,SQL_API_SQLEXECUTE);
        _set_func_exists(pfExists,SQL_API_SQLFETCH);
        _set_func_exists(pfExists,SQL_API_SQLFETCHSCROLL);
        _set_func_exists(pfExists,SQL_API_SQLFREECONNECT);
        _set_func_exists(pfExists,SQL_API_SQLFREEENV);
        _set_func_exists(pfExists,SQL_API_SQLFREEHANDLE);
        _set_func_exists(pfExists,SQL_API_SQLFREESTMT);
        _set_func_exists(pfExists,SQL_API_SQLGETCONNECTATTR);
        _set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
        _set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME);
        _set_func_exists(pfExists,SQL_API_SQLGETDATA);
        _set_func_exists(pfExists,SQL_API_SQLGETDESCFIELD);
        _set_func_exists(pfExists,SQL_API_SQLGETDESCREC);
        _set_func_exists(pfExists,SQL_API_SQLGETDIAGFIELD);
        _set_func_exists(pfExists,SQL_API_SQLGETDIAGREC);
        _set_func_exists(pfExists,SQL_API_SQLGETENVATTR);
        _set_func_exists(pfExists,SQL_API_SQLGETFUNCTIONS);
        _set_func_exists(pfExists,SQL_API_SQLGETINFO);
        _set_func_exists(pfExists,SQL_API_SQLGETSTMTATTR);
        _set_func_exists(pfExists,SQL_API_SQLGETSTMTOPTION);
        _set_func_exists(pfExists,SQL_API_SQLGETTYPEINFO);
        _set_func_exists(pfExists,SQL_API_SQLMORERESULTS);
        _set_func_exists(pfExists,SQL_API_SQLNUMPARAMS);
        _set_func_exists(pfExists,SQL_API_SQLNUMRESULTCOLS);
        _set_func_exists(pfExists,SQL_API_SQLPARAMDATA);
        _set_func_exists(pfExists,SQL_API_SQLPREPARE);
        _set_func_exists(pfExists,SQL_API_SQLPUTDATA);
        _set_func_exists(pfExists,SQL_API_SQLROWCOUNT);
        _set_func_exists(pfExists,SQL_API_SQLSETCONNECTATTR);
        _set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
        _set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
        _set_func_exists(pfExists,SQL_API_SQLSETDESCFIELD);
        _set_func_exists(pfExists,SQL_API_SQLSETDESCREC);
        _set_func_exists(pfExists,SQL_API_SQLSETENVATTR);
        _set_func_exists(pfExists,SQL_API_SQLSETPARAM);
        _set_func_exists(pfExists,SQL_API_SQLSETSTMTATTR);
        _set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
        _set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
        _set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
        _set_func_exists(pfExists,SQL_API_SQLTABLES);
        _set_func_exists(pfExists,SQL_API_SQLTRANSACT);

        return SQL_SUCCESS;
        break;
#endif
    case SQL_API_ALL_FUNCTIONS:
        tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetFunctions: "
                    "fFunction is SQL_API_ALL_FUNCTIONS\n");

        _set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLALLOCENV);
        _set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
        _set_func_exists(pfExists,SQL_API_SQLBINDCOL);
        _set_func_exists(pfExists,SQL_API_SQLCANCEL);
        _set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTES);
	_set_func_exists(pfExists,SQL_API_SQLCOLUMNS);
        _set_func_exists(pfExists,SQL_API_SQLCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLDRIVERCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLDATASOURCES);
        _set_func_exists(pfExists,SQL_API_SQLDESCRIBECOL);
        _set_func_exists(pfExists,SQL_API_SQLDISCONNECT);
        _set_func_exists(pfExists,SQL_API_SQLERROR);
        _set_func_exists(pfExists,SQL_API_SQLEXECDIRECT);
        _set_func_exists(pfExists,SQL_API_SQLEXECUTE);
        _set_func_exists(pfExists,SQL_API_SQLFETCH);
        _set_func_exists(pfExists,SQL_API_SQLFREECONNECT);
        _set_func_exists(pfExists,SQL_API_SQLFREEENV);
        _set_func_exists(pfExists,SQL_API_SQLFREESTMT);
        _set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
/*			_set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME); */
        _set_func_exists(pfExists,SQL_API_SQLGETDATA);
        _set_func_exists(pfExists,SQL_API_SQLGETFUNCTIONS);
        _set_func_exists(pfExists,SQL_API_SQLGETINFO);
        _set_func_exists(pfExists,SQL_API_SQLGETSTMTOPTION);
        _set_func_exists(pfExists,SQL_API_SQLGETTYPEINFO);
        _set_func_exists(pfExists,SQL_API_SQLMORERESULTS);
        _set_func_exists(pfExists,SQL_API_SQLNUMPARAMS);
        _set_func_exists(pfExists,SQL_API_SQLNUMRESULTCOLS);
        _set_func_exists(pfExists,SQL_API_SQLPARAMDATA);
        _set_func_exists(pfExists,SQL_API_SQLPREPARE);
        _set_func_exists(pfExists,SQL_API_SQLPUTDATA);
        _set_func_exists(pfExists,SQL_API_SQLROWCOUNT);
        _set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
/*
            _set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
            _set_func_exists(pfExists,SQL_API_SQLSETPARAM);
*/
        _set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
/*
            _set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
            _set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
*/
        _set_func_exists(pfExists,SQL_API_SQLTABLES);
	_set_func_exists(pfExists,SQL_API_SQLTRANSACT);
        return SQL_SUCCESS;
        break;
    case SQL_API_SQLALLOCCONNECT :
    case SQL_API_SQLALLOCENV :
    case SQL_API_SQLALLOCSTMT :
    case SQL_API_SQLBINDCOL :
    case SQL_API_SQLBINDPARAMETER :
    case SQL_API_SQLCANCEL :
    case SQL_API_SQLCOLATTRIBUTES :
    case SQL_API_SQLCOLUMNS :
    case SQL_API_SQLCONNECT :
    case SQL_API_SQLDRIVERCONNECT :
    case SQL_API_SQLDESCRIBECOL :
    case SQL_API_SQLDISCONNECT :
    case SQL_API_SQLENDTRAN :
    case SQL_API_SQLERROR :
    case SQL_API_SQLEXECDIRECT :
    case SQL_API_SQLEXECUTE :
    case SQL_API_SQLFETCH :
    case SQL_API_SQLFREECONNECT :
    case SQL_API_SQLFREEENV :
    case SQL_API_SQLFREESTMT :
    case SQL_API_SQLGETCONNECTOPTION :
/*		case SQL_API_SQLGETCURSORNAME : */
    case SQL_API_SQLGETDATA :
    case SQL_API_SQLGETFUNCTIONS :
    case SQL_API_SQLGETINFO :
    case SQL_API_SQLGETSTMTATTR :
    case SQL_API_SQLGETSTMTOPTION :
    case SQL_API_SQLGETTYPEINFO :
    case SQL_API_SQLMORERESULTS :
    case SQL_API_SQLNUMPARAMS :
    case SQL_API_SQLNUMRESULTCOLS :
    case SQL_API_SQLPARAMDATA :
    case SQL_API_SQLPREPARE :
    case SQL_API_SQLPUTDATA :
    case SQL_API_SQLROWCOUNT :
    case SQL_API_SQLSETCONNECTOPTION :
/*
        case SQL_API_SQLSETCURSORNAME :
        case SQL_API_SQLSETPARAM :
*/
    case SQL_API_SQLSETSTMTOPTION :
/*
        case SQL_API_SQLSPECIALCOLUMNS :
        case SQL_API_SQLSTATISTICS :
*/
    case SQL_API_SQLTABLES :
    case SQL_API_SQLTRANSACT :
#if (ODBCVER >= 0x300)
    case SQL_API_SQLALLOCHANDLE :
/*
        case SQL_API_SQLCLOSECURSOR :
        case SQL_API_SQLCOPYDESC :
        case SQL_API_SQLENDTRAN :
        case SQL_API_SQLFETCHSCROLL :
        case SQL_API_SQLGETCONNECTATTR :
*/
    case SQL_API_SQLFREEHANDLE :
/*
        case SQL_API_SQLGETDESCFIELD :
        case SQL_API_SQLGETDESCREC :
        case SQL_API_SQLGETDIAGFIELD :
        case SQL_API_SQLGETDIAGREC :
        case SQL_API_SQLGETENVATTR :
        case SQL_API_SQLGETSTMTATTR :
*/
	case SQL_API_SQLSETCONNECTATTR :
/*
        case SQL_API_SQLSETDESCFIELD :
        case SQL_API_SQLSETDESCREC :
        case SQL_API_SQLSETENVATTR :
        case SQL_API_SQLSETSTMTATTR :
*/

#endif
        *pfExists = 1; /* SQL_TRUE */
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
static char *strncpy_null(char *dst, const char *src, int len)
{
    int i;


    if (NULL != dst)
    {
        /*  Just in case, check for special lengths */
        if (len == SQL_NULL_DATA)
        {
            dst[0] = '\0';
            return NULL;
        }
        else if (len == SQL_NTS)
            len = strlen(src) + 1;

        for (i = 0; src[i] && i < len - 1; i++)
        {
            dst[i] = src[i];
        }

        if (len > 0)
        {
            dst[i] = '\0';
        }
    }
    return dst;
}

SQLRETURN SQL_API SQLGetInfo(
                            SQLHDBC            hdbc,
                            SQLUSMALLINT       fInfoType,
                            SQLPOINTER         rgbInfoValue,
                            SQLSMALLINT        cbInfoValueMax,
                            SQLSMALLINT FAR   *pcbInfoValue)
{
    const char *p = NULL;
    SQLSMALLINT *siInfoValue = (SQLSMALLINT *)rgbInfoValue;
    SQLUINTEGER *uiInfoValue = (SQLUINTEGER *)rgbInfoValue;

    CHECK_HDBC;

    switch (fInfoType)
    {
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
	case SQL_DRIVER_NAME: /* ODBC 2.0 */
        p = "libtdsodbc.so";
        break;
    case SQL_DRIVER_ODBC_VER:
        p = "02.00";
        break;
    case SQL_ACTIVE_STATEMENTS:
        *siInfoValue = 1;
        break;
    case SQL_SCROLL_OPTIONS:
        *uiInfoValue  = SQL_SO_FORWARD_ONLY | SQL_SO_STATIC;
        break;
    case SQL_SCROLL_CONCURRENCY:
        *uiInfoValue  = SQL_SCCO_READ_ONLY;
		break;
	case SQL_TXN_CAPABLE:
		/* transaction for DML and DDL */
		*siInfoValue = SQL_TC_ALL;
		break;
	case SQL_DEFAULT_TXN_ISOLATION:
		*uiInfoValue  = SQL_TXN_READ_COMMITTED;
		break;
	case SQL_FILE_USAGE:
		*uiInfoValue  = SQL_FILE_NOT_SUPPORTED;
		break;
	/* TODO support for other options */
	default:
		/* error HYC00*/
		tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetInfo: "
			"info option %d not supported\n", fInfoType);
		return SQL_ERROR;
    }

    if (p)
    {  /* char/binary data */
        int len = strlen(p);

        if (rgbInfoValue)
        {
            strncpy_null((char *)rgbInfoValue, p, (size_t)cbInfoValueMax);

            if (len >= cbInfoValueMax)
            {
                odbc_LogError("The buffer was too small for the result.");
                return( SQL_SUCCESS_WITH_INFO);
            }
        }
		if (pcbInfoValue)
			*pcbInfoValue = len;
    }

    return SQL_SUCCESS;
}

/* end pwillia6@csc.com.au 01/25/02 */

SQLRETURN SQL_API SQLGetStmtOption(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       fOption,
                                  SQLPOINTER         pvParam)
{
    SQLUINTEGER* piParam = (SQLUINTEGER*)pvParam;

    CHECK_HSTMT;

    switch (fOption)
    {
    case SQL_ROWSET_SIZE:
        *piParam = 1;
        break;
    default:
        tdsdump_log(TDS_DBG_INFO1, "odbc:SQLGetStmtOption: Statement option %d not implemented\n", fOption);
        odbc_LogError ("Statement option not implemented");
        return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

static void
odbc_upper_column_names(TDSSOCKET *tds)
{
	TDSRESULTINFO * resinfo;
	TDSCOLINFO *colinfo;
	int icol;
	char *p;

	if (!tds->res_info)
		return;

	resinfo = tds->res_info;
	for(icol=0; icol < resinfo->num_cols; ++icol) {
		colinfo = resinfo->columns[icol];
		/* upper case */
		/* TODO procedure */
		for(p = colinfo->column_name; *p; ++p)
			if ('a' <= *p && *p <= 'z')
				*p = *p & (~0x20);
	}
}

SQLRETURN SQL_API SQLGetTypeInfo(
                                SQLHSTMT           hstmt,
                                SQLSMALLINT        fSqlType)
{
struct _hstmt *stmt;
SQLRETURN res;
TDSSOCKET *tds;
TDS_INT row_type;
TDS_INT compute_id;
int varchar_pos = -1,n;

    CHECK_HSTMT;

	stmt = (struct _hstmt *) hstmt;
	tds = stmt->hdbc->tds_socket;

    /* For MSSQL6.5 and Sybase 11.9 sp_datatype_info work */
    /* FIXME what about early Sybase products ? */
    if (!fSqlType)
    {
        static const char *sql = "EXEC sp_datatype_info";
        if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, sql, strlen(sql)))
            return SQL_ERROR;
    }
    else
    {
        static const char sql_templ[] = "EXEC sp_datatype_info %d";
        char sql[sizeof(sql_templ)+20];
        sprintf(sql, sql_templ, fSqlType);
        if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, sql, strlen(sql)))
            return SQL_ERROR;
    }

redo:
	res = _SQLExecute(hstmt);
	
	odbc_upper_column_names(stmt->hdbc->tds_socket);

	if (TDS_IS_MSSQL(stmt->hdbc->tds_socket) || fSqlType != 12 || res != SQL_SUCCESS) 
		return res;

	/* Sybase return first nvarchar for char... and without length !!! */
	/* Some program use first entry so we discard all entry bfore varchar */
	n = 0;
	while(tds->res_info) {
		TDSRESULTINFO *resinfo;
		TDSCOLINFO *colinfo;
		char *name;
		/* if next is varchar leave next for SQLFetch */
		if (n == (varchar_pos-1))
			break;

		switch(tds_process_row_tokens(stmt->hdbc->tds_socket,
                                              &row_type, &compute_id)) {
		case TDS_NO_MORE_ROWS:
			while(tds->state==TDS_PENDING)
				tds_process_default_tokens(tds,tds_get_byte(tds));
			if (n>=varchar_pos && varchar_pos>0)
				goto redo;
			break;
		}
		if (!tds->res_info) break;
		++n;
		
		resinfo = tds->res_info;
		colinfo = resinfo->columns[0];
		name = (char*)(resinfo->current_row+colinfo->column_offset);
		/* skip nvarchar and sysname */
		if (colinfo->column_cur_size == 7 && memcmp("varchar", name, 7) == 0) {
			varchar_pos = n;
		}
	}
	return res;
}

SQLRETURN SQL_API SQLParamData(
                              SQLHSTMT           hstmt,
                              SQLPOINTER FAR    *prgbValue)
{
    struct _hstmt *stmt;
    struct _sql_param_info *param;

    CHECK_HSTMT;

    stmt = (struct _hstmt *) hstmt;
    if (stmt->prepared_query_need_bytes)
    {
        param = odbc_find_param(stmt, stmt->prepared_query_param_num);
        if (!param)
            return SQL_ERROR;

        *prgbValue = param->varaddr;
        return SQL_NEED_DATA;
    }

    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPutData(
                            SQLHSTMT           hstmt,
                            SQLPOINTER         rgbValue,
                            SQLINTEGER         cbValue)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;
    if (stmt->prepared_query && stmt->param_head)
    {
        SQLRETURN res = continue_parse_prepared_query(stmt, rgbValue, cbValue);
        if (SQL_NEED_DATA==res)
            return SQL_SUCCESS;
        if (SQL_SUCCESS!=res)
            return res;
    }

    return _SQLExecute(hstmt);
}


SQLRETURN SQL_API SQLSetConnectAttr(SQLHDBC       hdbc,
                                   SQLINTEGER     Attribute,
                                   SQLPOINTER     ValuePtr,
                                   SQLINTEGER     StringLength)
{
/* struct _hdbc *dbc = (struct _hdbc *) hdbc; */
SQLUINTEGER u_value = (SQLUINTEGER)ValuePtr;

	CHECK_HDBC;
	
	switch(Attribute) {
	case SQL_ATTR_AUTOCOMMIT:
		/* spinellia@acm.org */
		if (u_value == SQL_AUTOCOMMIT_ON)
			return change_autocommit( hdbc, 1);
		return change_autocommit( hdbc, 0);
		break;
/*	case SQL_ATTR_CONNECTION_TIMEOUT:
		dbc->tds_socket->connect_timeout = u_value;
		break; */
	}
	return SQL_ERROR;
}

SQLRETURN SQL_API SQLSetConnectOption(
                                     SQLHDBC            hdbc,
                                     SQLUSMALLINT       fOption,
                                     SQLUINTEGER        vParam)
{
    CHECK_HDBC;

    switch (fOption)
    {
	case SQL_AUTOCOMMIT:
		/* spinellia@acm.org */
		return SQLSetConnectAttr(hdbc,SQL_ATTR_AUTOCOMMIT,(SQLPOINTER)vParam,0);
    default:
        tdsdump_log(TDS_DBG_INFO1, "odbc:SQLSetConnectOption: Statement option %d not implemented\n", fOption);
        odbc_LogError ("Statement option not implemented");
        return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetStmtOption(
                                  SQLHSTMT           hstmt,
                                  SQLUSMALLINT       fOption,
                                  SQLUINTEGER        vParam)
{
    CHECK_HSTMT;

	switch (fOption)
	{
	case SQL_ROWSET_SIZE:
		/* Always 1 */
		break;
	case SQL_CURSOR_TYPE:
		if (vParam == SQL_CURSOR_FORWARD_ONLY)
			return SQL_SUCCESS;
		/* fall through */
	default:
		tdsdump_log(TDS_DBG_INFO1, "odbc:SQLSetStmtOption: Statement option %d not implemented\n", fOption);
		odbc_LogError ("Statement option not implemented");
		return SQL_ERROR;
	}

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSpecialColumns(
                                   SQLHSTMT           hstmt,
                                   SQLUSMALLINT       fColType,
                                   SQLCHAR FAR       *szCatalogName,
                                   SQLSMALLINT        cbCatalogName,
                                   SQLCHAR FAR       *szSchemaName,
                                   SQLSMALLINT        cbSchemaName,
                                   SQLCHAR FAR       *szTableName,
                                   SQLSMALLINT        cbTableName,
                                   SQLUSMALLINT       fScope,
                                   SQLUSMALLINT       fNullable)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLSpecialColumns: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLStatistics(
                               SQLHSTMT           hstmt,
                               SQLCHAR FAR       *szCatalogName,
                               SQLSMALLINT        cbCatalogName,
                               SQLCHAR FAR       *szSchemaName,
                               SQLSMALLINT        cbSchemaName,
                               SQLCHAR FAR       *szTableName,
                               SQLSMALLINT        cbTableName,
                               SQLUSMALLINT       fUnique,
                               SQLUSMALLINT       fAccuracy)
{
    CHECK_HSTMT;
    odbc_LogError ("SQLStatistics: function not implemented");
    return SQL_ERROR;
}

SQLRETURN SQL_API SQLTables(
                           SQLHSTMT           hstmt,
                           SQLCHAR FAR       *szCatalogName,
                           SQLSMALLINT        cbCatalogName,
                           SQLCHAR FAR       *szSchemaName,
                           SQLSMALLINT        cbSchemaName,
                           SQLCHAR FAR       *szTableName,
                           SQLSMALLINT        cbTableName,
                           SQLCHAR FAR       *szTableType,
                           SQLSMALLINT        cbTableType)
{
    char *query, *p;
    static const char sptables[] = "exec sp_tables ";
    int querylen, clen, slen, tlen, ttlen;
    int first = 1;
    struct _hstmt *stmt;
    SQLRETURN result;

    CHECK_HSTMT;

    stmt = (struct _hstmt *) hstmt;

    clen  = odbc_get_string_size(cbCatalogName, szCatalogName);
    slen  = odbc_get_string_size(cbSchemaName, szSchemaName);
    tlen  = odbc_get_string_size(cbTableName, szTableName);
    ttlen = odbc_get_string_size(cbTableType, szTableType);

    querylen = strlen(sptables) + clen + slen + tlen + ttlen + 40; /* a little padding for quotes and commas */
printf( "[PAH][%s][%d] Is query being free()'d?\n", __FILE__, __LINE__ );
    query = (char *) malloc(querylen);
    if (!query)
        return SQL_ERROR;
    p = query;

    strcpy(p, sptables);
    p += sizeof(sptables)-1;

    if (tlen)
    {
        *p++ = '"';
        strncpy(p, szTableName, tlen); *p+=tlen;
        *p++ = '"';
        first = 0;
    }
    if (slen)
    {
        if (!first) *p++ = ',';
        *p++ = '"';
        strncpy(p, szSchemaName, slen); *p+=slen;
        *p++ = '"';
        first = 0;
    }
    if (clen)
    {
        if (!first) *p++ = ',';
        *p++ = '"';
        strncpy(p, szCatalogName, clen); *p+=clen;
        *p++ = '"';
        first = 0;
    }
    if (ttlen)
    {
        if (!first) *p++ = ',';
        *p++ = '"';
        strncpy(p, szTableType, ttlen); *p+=ttlen;
        *p++ = '"';
        first = 0;
    }
    *p = '\0';
    /* fprintf(stderr,"\nquery = %s\n",query); */

    if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, query, p-query)) {
        free(query);
        return SQL_ERROR;
    }
    free(query);

    result  = _SQLExecute(hstmt);
    
	/* Sybase seem to return column name in lower case, 
	 * transform to uppercase 
	 * specification of ODBC and Perl test require these name be uppercase */
	odbc_upper_column_names(stmt->hdbc->tds_socket);

	return result;
}
static int sql_to_c_type_default ( int sql_type )
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

