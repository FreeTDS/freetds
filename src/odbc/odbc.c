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

static char  software_version[]   = "$Id: odbc.c,v 1.4 2001-10-26 11:16:26 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

static SQLSMALLINT _odbc_get_client_type(int srv_type);
static int _odbc_fix_literals(struct _hstmt *stmt);
static int _odbc_get_server_type(int clt_type);
static int _odbc_get_string_size(int size, char *str);
static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR *phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR *phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR *phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);

#define _MAX_ERROR_LEN 255
static char lastError[_MAX_ERROR_LEN+1];

static void LogError (const char* error)
{
   /*
    * Someday, I might make this store more than one error.
    */
	if (error) {
	   	strncpy (lastError, error, _MAX_ERROR_LEN);
   		lastError[_MAX_ERROR_LEN] = '\0'; /* in case we had a long message */
	}
}

/*
 * Driver specific connectionn information
 */

typedef struct
{
   struct _hdbc hdbc;
   ConnectParams* params;
} ODBCConnection;

/*
**
** Note: I *HATE* hungarian notation, it has to be the most idiotic thing
** I've ever seen. So, you will note it is avoided other than in the function
** declarations. "Gee, let's make our code totally hard to read and they'll
** beg for GUI tools"
** Bah!
*/

static SQLRETURN change_database (SQLHDBC hdbc, char *database)
{
   SQLRETURN ret;
   TDSSOCKET *tds;
   int marker;
   struct _hdbc *dbc = (struct _hdbc *) hdbc;
   char *query;

   tds = (TDSSOCKET *) dbc->tds_socket;
   query = (char *) malloc(strlen(database)+5);
   sprintf(query,"use %s", database);
   ret = tds_submit_query(tds,query);
   free(query);
   if (ret != TDS_SUCCEED) {
       LogError ("Could not change Database");
       return SQL_ERROR;
   }

   do {
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

   tds_set_server (dbc->tds_login, server);
   tds_set_user   (dbc->tds_login, user);
   tds_set_passwd (dbc->tds_login, passwd);
   dbc->tds_socket = (void *) tds_connect(dbc->tds_login, (void *)dbc);

   if (dbc->tds_socket == NULL)
   {
      LogError ("tds_connect failed");
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
   SQLCHAR FAR* server = NULL;
   SQLCHAR FAR* dsn = NULL;
   SQLCHAR FAR* uid = NULL;
   SQLCHAR FAR* pwd = NULL;
   SQLCHAR FAR* database = NULL;
   ConnectParams* params;
   SQLRETURN ret;

   strcpy (lastError, "");

   params = ((ODBCConnection*) hdbc)->params;

   if (!(dsn = ExtractDSN (params, szConnStrIn)))
   {
      LogError ("Could not find DSN in connect string");
      return SQL_ERROR;
   }
   else if (!LookupDSN (params, dsn))
   {
      LogError ("Could not find DSN in odbc.ini");
      return SQL_ERROR;
   }
   else 
   {
      SetConnectString (params, szConnStrIn);

      if (!(server = GetConnectParam (params, "Servername")))
      {
	 LogError ("Could not find Database parameter");
	 return SQL_ERROR;
      }
      else if (!(uid = GetConnectParam (params, "UID")))
      {
	 LogError ("Could not find UID parameter");
	 return SQL_ERROR;
      }
      else if (!(pwd = GetConnectParam (params, "PWD")))
      {
	 LogError ("Could not find PWD parameter");
	 return SQL_ERROR;
      }
   }
   if ((ret = do_connect (hdbc, server, uid, pwd))!=SQL_SUCCESS) 
   {
      return ret;
   }
   if (!(database = GetConnectParam (params, "Database")))
   {
	/* use the default database */
	return SQL_SUCCESS;
   }
   else
   {
       return change_database(hdbc, database);
   }
}

SQLRETURN SQL_API SQLBrowseConnect(
    SQLHDBC            hdbc,
    SQLCHAR FAR       *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLCHAR FAR       *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT FAR   *pcbConnStrOut)
{
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeParam(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT FAR   *pfSqlType,
    SQLUINTEGER FAR   *pcbParamDef,
    SQLSMALLINT FAR   *pibScale,
    SQLSMALLINT FAR   *pfNullable)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLExtendedFetch(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fFetchType,
    SQLINTEGER         irow,
    SQLUINTEGER FAR   *pcrow,
    SQLUSMALLINT FAR  *rgfRowStatus)
{
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNativeSql(
    SQLHDBC            hdbc,
    SQLCHAR FAR       *szSqlStrIn,
    SQLINTEGER         cbSqlStrIn,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStrMax,
    SQLINTEGER FAR    *pcbSqlStr)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pcpar)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT           hstmt,
    SQLUINTEGER        crow,
    SQLUINTEGER FAR   *pirow)
{
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       irow,
    SQLUSMALLINT       fOption,
    SQLUSMALLINT       fLock)
{
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDrivers(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR FAR       *szDriverDesc,
    SQLSMALLINT        cbDriverDescMax,
    SQLSMALLINT FAR   *pcbDriverDesc,
    SQLCHAR FAR       *szDriverAttributes,
    SQLSMALLINT        cbDrvrAttrMax,
    SQLSMALLINT FAR   *pcbDrvrAttr)
{
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLSetEnvAttr (
    SQLHENV EnvironmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER StringLength)
{
	return SQL_SUCCESS;
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

	stmt = (struct _hstmt *) hstmt;
	tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	colinfo = tds->res_info->columns[ipar-1];
        colinfo->varaddr = (char *)rgbValue;
        colinfo->column_bindtype = fCType;
        colinfo->column_bindlen = cbValueMax;
        colinfo->column_lenbind = pcbValue;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE InputHandle,
    	SQLHANDLE * OutputHandle)
{
	switch(HandleType) {
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
}
static SQLRETURN SQL_API _SQLAllocConnect(
    SQLHENV            henv,
    SQLHDBC FAR       *phdbc)
{
struct _henv *env;
ODBCConnection* dbc;

	env = (struct _henv *) henv;
        dbc = (SQLHDBC) malloc (sizeof (ODBCConnection));
        memset(dbc,'\0',sizeof (ODBCConnection));
	dbc->hdbc.henv=env;
	dbc->hdbc.tds_login= (void *) tds_alloc_login();
	dbc->params = NewConnectParams ();
	*phdbc=dbc;

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

        env = (SQLHENV) malloc(sizeof(struct _henv));
        memset(env,'\0',sizeof(struct _henv));
	*phenv=env;
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

	dbc = (struct _hdbc *) hdbc;

        stmt = (SQLHSTMT) malloc(sizeof(struct _hstmt));
        memset(stmt,'\0',sizeof(struct _hstmt));
	stmt->hdbc=hdbc;
	*phstmt = stmt;

	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT FAR      *phstmt)
{
	return _SQLAllocStmt(hdbc,phstmt);
}

SQLRETURN SQL_API SQLBindCol(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLSMALLINT        fCType,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
   /*
    * Copied this code from SQLBindParameter because it looks like it
    * belongs here. - Ken
    */
   TDSCOLINFO * colinfo;
   TDSRESULTINFO * resinfo;
   TDSSOCKET * tds;
   struct _hstmt *stmt;

   stmt = (struct _hstmt *) hstmt;
   tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
   colinfo = tds->res_info->columns[icol-1];
   colinfo->varaddr = (char *)rgbValue;
   colinfo->column_bindtype = fCType;
   colinfo->column_bindlen = cbValueMax;
   colinfo->column_lenbind = pcbValue;

   return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT           hstmt)
{
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
   SQLCHAR FAR* server = NULL;
   SQLCHAR FAR* database = NULL;
   SQLCHAR FAR* uid = NULL;
   SQLCHAR FAR* pwd = NULL;
   ConnectParams* params;
   SQLRETURN ret;

   strcpy (lastError, "");

   params = ((ODBCConnection*) hdbc)->params;

   if (!LookupDSN (params, szDSN))
   {
      LogError ("Could not find DSN in odbc.ini");
      return SQL_ERROR;
   }
   else if (!(server = GetConnectParam (params, "Servername")))
   {
      LogError ("Could not find Database parameter");
      return SQL_ERROR;
   }
   if (!szUID || !strlen(szUID)) {
      if (!(uid = GetConnectParam (params, "UID"))) {
	 	LogError ("Could not find UID parameter");
	 	return SQL_ERROR;
      }
   } else {
	uid = szUID;
   }
   if (!szAuthStr || !strlen(szAuthStr)) {
      if (!(pwd = GetConnectParam (params, "PWD")))
      {
	 	LogError ("Could not find PWD parameter");
	 	return SQL_ERROR;
      }
   } else {
	pwd = szAuthStr;
   }
   if ((ret = do_connect (hdbc, server, uid, pwd))!=SQL_SUCCESS)
   {
      return ret;
   }
   if (!(database = GetConnectParam (params, "Database")))
   {
	/* use the default database */
	return SQL_SUCCESS;
   }
   else
   {
       return change_database(hdbc, database);
   }
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

	tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	if (icol == 0 || icol > tds->res_info->num_cols)
	{
	   LogError ("SQLDescribeCol: Column out of range");
	   return SQL_ERROR;
	}
	colinfo = tds->res_info->columns[icol-1];

	if (szColName) {
		namelen = strlen(colinfo->column_name);
		cplen = namelen >= cbColNameMax ?
			cbColNameMax - 1 : namelen;
		strncpy(szColName,colinfo->column_name, cplen);
		for (i=0;i<cplen;i++) { 
			if (islower(szColName[i]))
				szColName[i] -= 0x20;
		}
		szColName[cplen]='\0';
		// fprintf(stderr,"\nsetting column name to %s %s\n", colinfo->column_name, szColName);
	}
    	if (pcbColName) {
		*pcbColName = strlen(colinfo->column_name);
	}
	if (pfSqlType) {
		*pfSqlType=_odbc_get_client_type(colinfo->column_type);
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

	stmt = (struct _hstmt *) hstmt;
	dbc = (struct _hdbc *) stmt->hdbc;
	tds = (TDSSOCKET *) dbc->tds_socket;


	/* dont check column index for these */
	switch(fDescType) {
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
	   LogError ("SQLDescribeCol: Query Returned No Result Set!");
		return SQL_ERROR;
	}

	if (icol == 0 || icol > tds->res_info->num_cols)
	{
	   LogError ("SQLDescribeCol: Column out of range");
	   return SQL_ERROR;
	}
	colinfo = tds->res_info->columns[icol-1];

	tdsdump_log(TDS_DBG_INFO1, "SQLColAttributes: fDescType is %d\n", fDescType);
	switch(fDescType) {
		case SQL_COLUMN_NAME:
			len = strlen(colinfo->column_name);
			cplen = len > cbDescMax ? cbDescMax : len;
			tdsdump_log(TDS_DBG_INFO2, "SQLColAttributes: copying %d bytes, len = %d, cbDescMax = %d\n",cplen, len, cbDescMax);
			strncpy(rgbDesc,colinfo->column_name,cplen);
			((char *)rgbDesc)[cplen]='\0';
			if (pcbDesc) {
				*pcbDesc = cplen;
			}
			break;
		case SQL_COLUMN_TYPE:
			*pfDesc=_odbc_get_client_type(colinfo->column_type);
			break;
		case SQL_COLUMN_LENGTH:
			*pfDesc = colinfo->column_size;
			break;
		case SQL_COLUMN_DISPLAY_SIZE:
			switch(_odbc_get_client_type(colinfo->column_type)) {
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
				case SQL_DATETIME:
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
   
   if (strlen (lastError) > 0)
   {
      strcpy (szSqlState, "08001");
      strcpy (szErrorMsg, lastError);
      if (pcbErrorMsg)
	 *pcbErrorMsg = strlen (lastError);
      if (pfNativeError)
	 *pfNativeError = 1;

      result = SQL_SUCCESS;
      strcpy (lastError, "");
   }

   return result;
}

static SQLRETURN SQL_API _SQLExecute( SQLHSTMT hstmt)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
int ret;
TDSSOCKET *tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
   
   fprintf(stderr,"query = %s\n",stmt->query);
   _odbc_fix_literals(stmt);

   if (!(tds_submit_query(tds, stmt->query)==TDS_SUCCEED)) {
	LogError (tds->msg_info->message);
   	return SQL_ERROR;
   }
   /* does anyone know how ODBC deals with multiple result sets? */
   ret = tds_process_result_tokens(tds);
   if (ret==TDS_NO_MORE_RESULTS) {
	/* DBD::ODBC expect SQL_SUCCESS on non-result returning queries */
   	return SQL_SUCCESS;
   } else if (ret==TDS_SUCCEED) {
   	return SQL_SUCCESS;
   }
}

SQLRETURN SQL_API SQLExecDirect(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
int ret;

   strcpy(stmt->query, szSqlStr);

   return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT           hstmt)
{
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
unsigned char *src;
int srclen;

	stmt=(struct _hstmt *)hstmt;

	tds = stmt->hdbc->tds_socket;
	
 	ret = tds_process_row_tokens(stmt->hdbc->tds_socket);
	if (ret==TDS_NO_MORE_ROWS) {
		return SQL_NO_DATA_FOUND;
	} 
	resinfo = tds->res_info;
	if (!resinfo) {
		return SQL_NO_DATA_FOUND;
	}
	for (i=0;i<resinfo->num_cols;i++) {
      		colinfo = resinfo->columns[i];
		if (colinfo->varaddr) {
			if (is_blob_type(colinfo->column_type)) {
				src = colinfo->column_textvalue;
				srclen = colinfo->column_textsize + 1;
			} else {
				src = &resinfo->current_row[colinfo->column_offset];
				srclen = -1;
			}
			len = tds_convert(
         		tds_get_conversion_type(colinfo->column_type, colinfo->column_size),
			src,
			srclen, 
			_odbc_get_server_type(colinfo->column_bindtype), 
			colinfo->varaddr, 
			colinfo->column_bindlen);
/*
			strcpy(colinfo->varaddr, 
			&resinfo->current_row[colinfo->column_offset]);
*/
		}
		if (colinfo->column_lenbind) {
			*((SQLINTEGER *)colinfo->column_lenbind)=len;
		}
	}
	if (ret==TDS_SUCCEED) 
		return SQL_SUCCESS;
	else {
		return SQL_ERROR;
	}
}

SQLRETURN SQL_API SQLFreeHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE Handle)
{
	switch(HandleType) {
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
}

static SQLRETURN SQL_API _SQLFreeConnect(
    SQLHDBC            hdbc)
{
   ODBCConnection* dbc = (ODBCConnection*) hdbc;


   FreeConnectParams (dbc->params);
   free (dbc);

   return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC            hdbc)
{
	return _SQLFreeConnect(hdbc);
}

static SQLRETURN SQL_API _SQLFreeEnv(
    SQLHENV            henv)
{
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV            henv)
{
	return _SQLFreeEnv(henv);
}

static SQLRETURN SQL_API _SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
TDSSOCKET * tds;
struct _hstmt *stmt=(struct _hstmt *)hstmt;

   if (fOption==SQL_DROP) {
   	free (hstmt);
   } else if (fOption==SQL_CLOSE) {
	tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	/* 
	** FIX ME -- otherwise make sure the current statement is complete
	*/
	if (tds->state==TDS_PENDING) {
		tds_send_cancel(tds);
		tds_process_cancel(tds);
	}
   } else {
	tdsdump_log(TDS_DBG_ERROR, "SQLFreeStmt: Unknown option %d\n", fOption);
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
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLength)
{
   return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCursor,
    SQLSMALLINT        cbCursorMax,
    SQLSMALLINT FAR   *pcbCursor)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pccol)
{
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
struct _hstmt *stmt;
	
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
	      LogError (tds->msg_info->message);
           else
	      LogError ("SQLNumResultCols: resinfo is NULL");

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

   if (cbSqlStr!=SQL_NTS) {
	strncpy(stmt->query, szSqlStr, cbSqlStr);
	stmt->query[cbSqlStr]='\0';
   } else {
   	strcpy(stmt->query, szSqlStr);
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

/* 7/28/2001 begin l@poliris.com */
	stmt=(struct _hstmt *)hstmt;
	tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo == NULL) {
		*pcrow = 0;
		return SQL_SUCCESS;
/*
		if (tds && tds->msg_info && tds->msg_info->message)
			LogError (tds->msg_info->message);
		else
			LogError ("SQLRowCount: resinfo is NULL");
	
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLUSMALLINT       fType)
{
	return SQL_SUCCESS;
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColumns(
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
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetConnectOption(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
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
unsigned char *src;
int srclen;

	stmt = (struct _hstmt *) hstmt;
	tds = (TDSSOCKET *) stmt->hdbc->tds_socket;
	resinfo = tds->res_info;
	if (icol == 0 || icol > tds->res_info->num_cols)
	{
	   LogError ("SQLGetData: Column out of range");
	   return SQL_ERROR;
	}
	colinfo = resinfo->columns[icol-1];

	if (tds_get_null(resinfo->current_row, icol-1)) {
		*pcbValue=SQL_NULL_DATA;
	} else {
		if (is_blob_type(colinfo->column_type)) {
			src = colinfo->column_textvalue;
			srclen = colinfo->column_textsize + 1;
		} else {
			src = &resinfo->current_row[colinfo->column_offset];
			srclen = -1;
		}
		*pcbValue=tds_convert(
      tds_get_conversion_type(colinfo->column_type, colinfo->column_size),
		src,
		srclen, 
		_odbc_get_server_type(fCType), 
		rgbValue,
		cbValueMax);
	}
	/*
	memcpy(rgbValue,&resinfo->current_row[colinfo->column_offset],
		colinfo->column_size);
	*/
	return 0;
}

SQLRETURN SQL_API SQLGetFunctions(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fFunction,
    SQLUSMALLINT FAR  *pfExists)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fInfoType,
    SQLPOINTER         rgbInfoValue,
    SQLSMALLINT        cbInfoValueMax,
    SQLSMALLINT FAR   *pcbInfoValue)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetStmtOption(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetTypeInfo(
    SQLHSTMT           hstmt,
    SQLSMALLINT        fSqlType)
{
struct _hstmt *stmt;

	stmt = (struct _hstmt *) hstmt;
	if (!fSqlType) {
   		strcpy(stmt->query, "SELECT * FROM tds_typeinfo");
	} else {
			sprintf(stmt->query, "SELECT * FROM tds_typeinfo WHERE SQL_DATA_TYPE = %d", fSqlType);
		}
		
		return _SQLExecute(hstmt);
	}

	SQLRETURN SQL_API SQLParamData(
	    SQLHSTMT           hstmt,
	    SQLPOINTER FAR    *prgbValue)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLPutData(
	    SQLHSTMT           hstmt,
	    SQLPOINTER         rgbValue,
	    SQLINTEGER         cbValue)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLSetConnectOption(
	    SQLHDBC            hdbc,
	    SQLUSMALLINT       fOption,
	    SQLUINTEGER        vParam)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLSetStmtOption(
	    SQLHSTMT           hstmt,
	    SQLUSMALLINT       fOption,
	    SQLUINTEGER        vParam)
	{
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
		return SQL_SUCCESS;
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
		return SQL_SUCCESS;
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

		stmt = (struct _hstmt *) hstmt;

		clen = _odbc_get_string_size(cbCatalogName, szCatalogName);
		slen = _odbc_get_string_size(cbSchemaName, szSchemaName);
		tlen = _odbc_get_string_size(cbTableName, szTableName);
		ttlen = _odbc_get_string_size(cbTableType, szTableType);

		querylen = strlen(sptables) + clen + slen + tlen + ttlen + 40; /* a little padding for quotes and commas */
		query = (char *) malloc(querylen);
		p = query;

		strcpy(p, sptables);
		p += strlen(sptables);

		if (tlen) {
			*p++ = '"';
			strncpy(p, szTableName, tlen); *p+=tlen;
			*p++ = '"';
			first = 0;
		}
		if (slen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szSchemaName, slen); *p+=slen;
			*p++ = '"';
			first = 0;
		}
		if (clen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szCatalogName, clen); *p+=clen;
			*p++ = '"';
			first = 0;
		}
		if (ttlen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szTableType, ttlen); *p+=ttlen;
			*p++ = '"';
			first = 0;
		}
		*p++ = '\0';
		// fprintf(stderr,"\nquery = %s\n",query);

	strcpy(stmt->query, query);
	return _SQLExecute(hstmt);
}


SQLRETURN SQL_API SQLDataSources(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR FAR       *szDSN,
    SQLSMALLINT        cbDSNMax,
    SQLSMALLINT FAR   *pcbDSN,
    SQLCHAR FAR       *szDescription,
    SQLSMALLINT        cbDescriptionMax,
    SQLSMALLINT FAR   *pcbDescription)
{
	return SQL_SUCCESS;
}
static int _odbc_fix_literals(struct _hstmt *stmt)
{
char tmp[4096],begin_tag[11];
char *s, *d, *p;
int i, quoted = 0, find_end = 0;
char quote_char;

        s=stmt->query;
        d=tmp;
        while (*s) {
		if (!quoted && (*s=='"' || *s=='\'')) {
			quoted = 1;
			quote_char = *s;
		} else if (quoted && *s==quote_char) {
			quoted = 0;
		}
		if (!quoted && find_end && *s=='}') {
			s++; /* ignore the end of tag */
		} else if (!quoted && *s=='{') {
			for (p=s,i=0;*p && *p!=' ';p++) i++;
			if (i>10) {
				/* garbage */
				*d++=*s++;
			} else {
				strncpy(begin_tag, s, i);
				begin_tag[i] = '\0';
				/* printf("begin tag %s\n", begin_tag); */
				s += i;
				find_end = 1;
			}
		} else {
			*d++=*s++;	
		}
        }
	*d='\0';
	strcpy(stmt->query,tmp);
}

static int _odbc_get_string_size(int size, char *str)
{
	if (!str) {
		return 0;
	}
	if (size==SQL_NTS) {
		return strlen(str);
	} else {
		return size;
	}
}
static int _odbc_get_server_type(int clt_type)
{
	switch (clt_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
		return SYBCHAR;
	case SQL_BIT:
		return SYBBIT;
	case SQL_TINYINT:
		return SYBINT1;
	case SQL_SMALLINT:
		return SYBINT2;
	case SQL_INTEGER:
		return SYBINT4;
	case SQL_DOUBLE:
		return SYBFLT8;
	case SQL_DECIMAL:
		return SYBDECIMAL;
	case SQL_NUMERIC:
		return SYBNUMERIC;
	case SQL_FLOAT:
		return SYBREAL;
	}
}
static SQLSMALLINT _odbc_get_client_type(int srv_type)
{
	switch (srv_type) {
	case SYBCHAR:
		return SQL_CHAR;
        case SYBVARCHAR:
		return SQL_VARCHAR;
         case SYBTEXT:
		return SQL_LONGVARCHAR;
		break;
	case SYBBIT:
		return SQL_BIT;
         case SYBINT4:
      		return SQL_INTEGER;
		break;
         case SYBINT2:
      		return SQL_SMALLINT;
		break;
         case SYBINT1:
      		return SQL_TINYINT;
		break;
         case SYBREAL:
      		return SQL_FLOAT;
		break;
         case SYBFLT8:
      		return SQL_DOUBLE;
		break;
         case SYBMONEY:
		break;
         case SYBMONEY4:
		break;
         case SYBDATETIME:
		return SQL_DATE;
		break;
         case SYBDATETIME4:
		return SQL_DATE;
		break;
         case SYBDATETIMN:
		return SQL_DATE;
		break;
	}
}
