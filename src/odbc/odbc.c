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

#include <config.h>

#ifdef UNIXODBC
    #include <sql.h>
    #include <sqlext.h>
#else
    #include "isql.h"
    #include "isqlext.h"
#endif

#include <tdsodbc.h>
#include <tds.h>

#include <string.h>
#include <stdio.h>

#include "connectparams.h"
#include "odbc_util.h"
#include "convert_tds2sql.h"
#include "prepare_query.h"

static char  software_version[]   = "$Id: odbc.c,v 1.37 2002-07-15 03:29:58 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
    no_unused_var_warn};

static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR *phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR *phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR *phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);
static char *strncpy_null(char *dst, const char *src, int len);


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

static SQLRETURN do_connect (
                            SQLHDBC hdbc,
                            SQLCHAR FAR *server,
                            SQLCHAR FAR *user,
                            SQLCHAR FAR *passwd
                            )
{
    struct _hdbc *dbc = (struct _hdbc *) hdbc;
    struct _henv *env = dbc->henv;

    tds_set_server (dbc->tds_login, (char*)server);
    tds_set_user   (dbc->tds_login, (char*)user);
    tds_set_passwd (dbc->tds_login, (char*)passwd);
    dbc->tds_socket = (void *) tds_connect(dbc->tds_login, env->tds_ctx, (void *)dbc);

    if (dbc->tds_socket == NULL)
    {
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
    char szDataSourceName[FILENAME_MAX];
    char szServer[FILENAME_MAX];
    char szDatabase[FILENAME_MAX];
    char szUID[FILENAME_MAX];
    char szPWD[FILENAME_MAX];
    SQLRETURN ret;

    CHECK_HDBC;

    odbc_LogError("");

    tdoParseConnectString( szConnStrIn, 
                           szDataSourceName,
                           szServer,
                           szDatabase,
                           szUID,
                           szPWD );

    if ( !(*szServer) )
    {
        odbc_LogError( "Could not find Servername parameter" );
        return SQL_ERROR;
    }

    if ( !(*szUID) )
    {
        odbc_LogError( "Could not find UID parameter" );
        return SQL_ERROR;
    }

    if ( !(*szPWD) )
    {
        odbc_LogError( "Could not find PWD parameter" );
        return SQL_ERROR;
    }

    if ( (ret = do_connect( hdbc, szServer, szUID, szPWD )) != SQL_SUCCESS )
    {
        return ret;
    }

    if ( *szDatabase )
    {
        return change_database( hdbc, szDatabase );
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

    CHECK_HSTMT;

    stmt=(struct _hstmt *)hstmt;
    tds = stmt->hdbc->tds_socket;

    /* try to go to the next recordset */
    switch (tds_process_result_tokens(tds))
    {
    case TDS_NO_MORE_RESULTS:
        odbc_set_return_status(stmt);
        return SQL_NO_DATA_FOUND;
    case TDS_SUCCEED:
        return SQL_SUCCESS;
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
    TDSCOLINFO * colinfo;
    TDSRESULTINFO * resinfo;
    TDSSOCKET * tds;
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
    cur->param_sqltype = fSqlType;
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

    env = (struct _henv*) malloc(sizeof(struct _henv));
    if (!env)
        return SQL_ERROR;
    memset(env,'\0',sizeof(struct _henv));
	env->tds_ctx = tds_alloc_context();
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
    TDSCOLINFO * colinfo;
    TDSRESULTINFO * resinfo;
    TDSSOCKET * tds;
    struct _hstmt *stmt;
    struct _sql_bind_info *cur, *prev, *newitem;

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
    char szDataSourceName[FILENAME_MAX];
    char szServer[FILENAME_MAX];
    char szDatabase[FILENAME_MAX];
    char szUser[FILENAME_MAX];
    char szPassword[FILENAME_MAX];
    SQLRETURN   nRetVal;

    CHECK_HDBC;

    odbc_LogError("");

    /* data source name */
    if ( szDSN || (*szDSN) )
        strncpy( szDataSourceName, szDSN, FILENAME_MAX );
    else
        strcpy( szDataSourceName, "DEFAULT" );

    /* user id */
    if ( szUID && (*szUID) )
        strcpy( szUser, szUID );
    else
    {
        if ( SQLGetPrivateProfileString( szDataSourceName, "UID", "", szUser, FILENAME_MAX, "odbc.ini" ) < 1 )
        {
            odbc_LogError( "Could not determine UID." );
            return SQL_ERROR;
        }
    }

    /* password */
    if ( szAuthStr && (*szAuthStr) )
        strcpy( szPassword, szAuthStr );
    else
    {
        if ( SQLGetPrivateProfileString( szDataSourceName, "PWD", "", szPassword, FILENAME_MAX, "odbc.ini" ) < 1 )
        {
            odbc_LogError( "Could not determine PWD." );
            return SQL_ERROR;
        }
    }

    /* server */
    *szServer = '\0';
    SQLGetPrivateProfileString( szDataSourceName, "Servername", "localhost", szServer, FILENAME_MAX, "odbc.ini" );

    /* DO IT */
    if ( (nRetVal = do_connect( hdbc, szServer, szUser, szPassword)) != SQL_SUCCESS )
        return nRetVal;

    /* database */
    if ( SQLGetPrivateProfileString( szDataSourceName, "Database", "", szDatabase, FILENAME_MAX, "odbc.ini" ) > 0 )
        return change_database( hdbc, szDatabase );

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
    TDSRESULTINFO * resinfo;
    TDSCOLINFO *colinfo;
    int cplen, namelen, i;
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;

    tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    if (icol == 0 || icol > tds->res_info->num_cols)
    {
        odbc_LogError ("SQLDescribeCol: Column out of range");
        return SQL_ERROR;
    }
    colinfo = tds->res_info->columns[icol-1];

    if (szColName)
    {
        namelen = strlen(colinfo->column_name);
        cplen = namelen >= cbColNameMax ?
                cbColNameMax - 1 : namelen;
        strncpy(szColName,colinfo->column_name, cplen);
        for (i=0;i<cplen;i++)
        {
            if (islower(szColName[i]))
                szColName[i] -= 0x20;
        }
        szColName[cplen]='\0';
        /* fprintf(stderr,"\nsetting column name to %s %s\n", colinfo->column_name, szColName); */
    }
    if (pcbColName)
    {
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
        len = strlen(colinfo->column_name);
        cplen = len > cbDescMax ? cbDescMax : len;
        tdsdump_log(TDS_DBG_INFO2, "SQLColAttributes: copying %d bytes, len = %d, cbDescMax = %d\n",cplen, len, cbDescMax);
        strncpy(rgbDesc,colinfo->column_name,cplen);
        ((char *)rgbDesc)[cplen]='\0';
        if (pcbDesc)
        {
            *pcbDesc = cplen;
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
    case SQL_COLUMN_PRECISION: // this section may be wrong
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
            *pfDesc = colinfo->column_size;
            break;
        case SQL_INTEGER:
            *pfDesc = 8;
            break;
        case SQL_SMALLINT:
            *pfDesc = 6;
            break;
        case SQL_TINYINT:
            *pfDesc = 4;
            break;
        case SQL_DATE:
            *pfDesc = 19;
            break;
        }
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

static SQLRETURN SQL_API 
_SQLExecute( SQLHSTMT hstmt)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;
    int ret;
    TDSSOCKET *tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
    TDSCOLINFO *colinfo;

    CHECK_HSTMT;

    stmt->row = 0;

    if (!(tds_submit_query(tds, stmt->query)==TDS_SUCCEED))
    {
        odbc_LogError (tds->msg_info->message);
        return SQL_ERROR;
    }

    ret = tds_process_result_tokens(tds);
    if (ret==TDS_NO_MORE_RESULTS)
    {
        return SQL_SUCCESS;
    }
    else if (ret==TDS_SUCCEED)
    {
        return SQL_SUCCESS;
    }
    else
    {
        return SQL_ERROR;
    }
}

SQLRETURN SQL_API SQLExecDirect(
                               SQLHSTMT           hstmt,
                               SQLCHAR FAR       *szSqlStr,
                               SQLINTEGER         cbSqlStr)
{
    struct _hstmt *stmt = (struct _hstmt *) hstmt;
    int ret;

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
    struct _hstmt *stmt = (struct _hstmt *) hstmt;

    CHECK_HSTMT;
    if (SQL_SUCCESS!=prepare_call(stmt))
        return SQL_ERROR;

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
                colinfo->varaddr = cur->varaddr;
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

    ret = tds_process_row_tokens(stmt->hdbc->tds_socket);
    if (ret==TDS_NO_MORE_ROWS)
    {
        return SQL_NO_DATA_FOUND;
    }
    resinfo = tds->res_info;
    if (!resinfo)
    {
        return SQL_NO_DATA_FOUND;
    }
    for (i=0;i<resinfo->num_cols;i++)
    {
        colinfo = resinfo->columns[i];
        colinfo->column_text_sqlgetdatapos = 0;
        if (colinfo->varaddr && !tds_get_null(resinfo->current_row, i))
        {
            if (is_blob_type(colinfo->column_type))
            {
                src = colinfo->column_textvalue;
                srclen = colinfo->column_textsize + 1;
            }
            else
            {
                src = (TDS_CHAR*)&resinfo->current_row[colinfo->column_offset];
                srclen = colinfo->column_size;
            }
            len = convert_tds2sql(context, 
                                  tds_get_conversion_type(colinfo->column_type, colinfo->column_size),
                                  src,
                                  srclen, 
                                  colinfo->column_bindtype, 
                                  colinfo->varaddr, 
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
    if (ret==TDS_SUCCEED)
        return SQL_SUCCESS;
    else
    {
        return SQL_ERROR;
    }
}


#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API SQLFreeHandle(
                               SQLSMALLINT HandleType,
                               SQLHANDLE Handle)
{
    switch (HandleType)
    {
    case SQL_HANDLE_STMT:
        _SQLFreeStmt(Handle,SQL_DROP);
        break;
    case SQL_HANDLE_DBC:
        _SQLFreeConnect(Handle);
        break;
    case SQL_HANDLE_ENV:
        _SQLFreeEnv(Handle);
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
        if (tds->state==TDS_PENDING)
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
    odbc_LogError ("SQLGetStmtAttr: function not implemented");
    return SQL_ERROR;
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
    char quoting,*p;
    struct _hstmt *stmt=(struct _hstmt *)hstmt;

    CHECK_HSTMT;

    if (SQL_SUCCESS!=odbc_set_stmt_prepared_query(stmt, (char*)szSqlStr, cbSqlStr))
        return SQL_ERROR;

    /* count parameters */
    stmt->param_count = 0;
    for (p=stmt->prepared_query;*p;++p)
        switch (*p)
        {
        /* skip quoted chars */
        case '\'':
        case '"':
            quoting = *p;
            while (*++p)
                if (*p == quoting)
                {
                    ++p;
                    break;
                }
            break;
        case '?':
            ++stmt->param_count;
            break;
        }

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

SQLRETURN SQL_API SQLTransact(
                             SQLHENV            henv,
                             SQLHDBC            hdbc,
                             SQLUSMALLINT       fType)
{
    CHECK_HENV;
    CHECK_HDBC;
    odbc_LogError ("SQLTransact: function not implemented");
    return SQL_ERROR;
}


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

/* printf( "[PAH][%s][%d]%s\n", __FILE__, __LINE__, szQuery ); */

    if ( SQL_SUCCESS != odbc_set_stmt_query( stmt, szQuery, strlen( szQuery ) ) )
        return SQL_ERROR;

    return _SQLExecute( hstmt );
}

SQLRETURN SQL_API SQLGetConnectOption(
                                     SQLHDBC            hdbc,
                                     SQLUSMALLINT       fOption,
                                     SQLPOINTER         pvParam)
{
    SQLUINTEGER *piParam = (SQLUINTEGER *) pvParam;

    CHECK_HDBC;
    switch (fOption)
    {
/*		case :
            break; */
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
    int nSybType;

    CHECK_HSTMT;

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
        if (is_blob_type(colinfo->column_type))
        {
            if (colinfo->column_text_sqlgetdatapos >= colinfo->column_textsize)
                return SQL_NO_DATA_FOUND;
            src = colinfo->column_textvalue + colinfo->column_text_sqlgetdatapos;
            srclen = colinfo->column_textsize + 1 - colinfo->column_text_sqlgetdatapos;
        }
        else
        {
            src = (TDS_CHAR*)&resinfo->current_row[colinfo->column_offset];
            srclen = -1;
        }
        nSybType = tds_get_conversion_type( colinfo->column_type, colinfo->column_size );
        *pcbValue=convert_tds2sql(context, 
                                  nSybType,
                                  src,
                                  colinfo->column_size /* srclen */, 
                                  fCType, 
                                  rgbValue,
                                  cbValueMax);

        if (is_blob_type(colinfo->column_type))
        {
            colinfo->column_text_sqlgetdatapos += *pcbValue;
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
    int i;

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
/*			_set_func_exists(pfExists,SQL_API_SQLTRANSACT); */
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
/*		case SQL_API_SQLTRANSACT : */
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
        case SQL_API_SQLSETCONNECTATTR :
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
/*// strncpy copies up to len characters, and doesn't terminate */
/*// the destination string if src has len characters or more. */
/*// instead, I want it to copy up to len-1 characters and always */
/*// terminate the destination string. */
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
    char *p = NULL;
    SQLSMALLINT *siInfoValue = (SQLSMALLINT *)rgbInfoValue;
    SQLUINTEGER *uiInfoValue = (SQLUINTEGER *)rgbInfoValue;

    CHECK_HDBC;

    switch (fInfoType)
    {
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
    default:
        tdsdump_log(TDS_DBG_FUNC, "odbc:SQLGetInfo: "
                    "info option %d not supported\n", fInfoType);
        break;
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

SQLRETURN SQL_API SQLGetTypeInfo(
                                SQLHSTMT           hstmt,
                                SQLSMALLINT        fSqlType)
{
    struct _hstmt *stmt;

    CHECK_HSTMT;

    stmt = (struct _hstmt *) hstmt;
    /* TODO for MSSQL6.5 + use sp_datatype_info fSqlType */
    if (!fSqlType)
    {
        static const char *sql = "SELECT * FROM tds_typeinfo";
        if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, sql, strlen(sql)))
            return SQL_ERROR;
    }
    else
    {
        static const char *sql_templ = "SELECT * FROM tds_typeinfo WHERE SQL_DATA_TYPE = %d";
        char sql[sizeof(*sql_templ)+20];
        sprintf(sql, sql_templ, fSqlType);
        if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, sql, strlen(sql)))
            return SQL_ERROR;
    }

    return _SQLExecute(hstmt);
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

SQLRETURN SQL_API SQLSetConnectOption(
                                     SQLHDBC            hdbc,
                                     SQLUSMALLINT       fOption,
                                     SQLUINTEGER        vParam)
{
    CHECK_HDBC;

    switch (fOption)
    {
/*        case :
                 break;*/
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
    char *sptables = "exec sp_tables ";
    int querylen, clen, slen, tlen, ttlen;
    int first = 1;
    struct _hstmt *stmt;

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
    p += strlen(sptables);

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
    return _SQLExecute(hstmt);
}
