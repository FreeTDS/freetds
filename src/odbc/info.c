/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001  Brian Bruns
 * Copyright (C) 2002-2012  Frediano Ziglio
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

/**
 * \file
 * SQLGetInfo and SQLGetFunctions implementation
 */

#include <config.h>

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/utils.h>
#include <freetds/odbc.h>
#include <odbcss.h>

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
odbc_log_unimplemented_type(const char function_name[], int fType)
{
	const char *name, *category;

	switch (fType) {
#ifdef SQL_ALTER_SCHEMA
	case SQL_ALTER_SCHEMA:
		name = "SQL_ALTER_SCHEMA";
		category = "Supported SQL";
		break;
#endif
#ifdef SQL_ANSI_SQL_DATETIME_LITERALS
	case SQL_ANSI_SQL_DATETIME_LITERALS:
		name = "SQL_ANSI_SQL_DATETIME_LITERALS";
		category = "Supported SQL";
		break;
#endif
	case SQL_COLLATION_SEQ:
		name = "SQL_COLLATION_SEQ";
		category = "Data Source Information";
		break;
	case SQL_CONVERT_BIGINT:
		name = "SQL_CONVERT_BIGINT";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_DATE:
		name = "SQL_CONVERT_DATE";
		category = "Conversion Information";
		break;
	case SQL_CONVERT_DOUBLE:
		name = "SQL_CONVERT_DOUBLE";
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
	case SQL_DM_VER:
		name = "SQL_DM_VER";
		category = "Added for ODBC 3.x";
		break;
	case SQL_DRIVER_HLIB:
		name = "SQL_DRIVER_HLIB";
		category = "Driver Information";
		break;
#ifdef SQL_ODBC_STANDARD_CLI_CONFORMANCE
	case SQL_ODBC_STANDARD_CLI_CONFORMANCE:
		name = "SQL_ODBC_STANDARD_CLI_CONFORMANCE";
		category = "Driver Information";
		break;
#endif
	case SQL_USER_NAME:
		name = "SQL_USER_NAME";
		category = "Data Source Information";
		break;
		/* TODO extension SQL_INFO_SS_NETLIB_NAME ?? */
	default:
		name = "unknown";
		category = "unknown";
		break;
	}

	tdsdump_log(TDS_DBG_INFO1, "not implemented: %s: option/type %d(%s) [category %s]\n", function_name, fType, name, category);

	return;
}

SQLRETURN ODBC_PUBLIC ODBC_API
SQLGetFunctions(SQLHDBC hdbc, SQLUSMALLINT fFunction, SQLUSMALLINT FAR *pfExists)
{
	int i;

	ODBC_ENTER_HDBC;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetFunctions: fFunction is %d\n", fFunction);
	switch (fFunction) {
#if (ODBCVER >= 0x0300)
	case SQL_API_ODBC3_ALL_FUNCTIONS:
		for (i = 0; i < SQL_API_ODBC3_ALL_FUNCTIONS_SIZE; ++i) {
			pfExists[i] = 0;
		}

		/*
		 * every api available are contained in a macro
		 * all these macro begin with API followed by 2 letter
		 * first letter mean pre ODBC 3 (_) or ODBC 3 (3)
		 * second letter mean implemented (X) or unimplemented (_)
		 * You should copy these macro 3 times... not very good
		 * but works. Perhaps best method is build the bit array statically
		 * and then use it but I don't know how to build it...
		 */
#undef ODBC_ALL_API
#undef ODBC_COLATTRIBUTE

#if SQL_API_SQLCOLATTRIBUTE != SQL_API_SQLCOLATTRIBUTES
#define ODBC_COLATTRIBUTE(s) s
#else
#define ODBC_COLATTRIBUTE(s)
#endif

#define ODBC_ALL_API \
	API_X(SQL_API_SQLALLOCCONNECT)\
	API_X(SQL_API_SQLALLOCENV)\
	API3X(SQL_API_SQLALLOCHANDLE)\
	API_X(SQL_API_SQLALLOCSTMT)\
	API_X(SQL_API_SQLBINDCOL)\
	API_X(SQL_API_SQLBINDPARAM)\
	API_X(SQL_API_SQLBINDPARAMETER)\
	API__(SQL_API_SQLBROWSECONNECT)\
	API3_(SQL_API_SQLBULKOPERATIONS)\
	API_X(SQL_API_SQLCANCEL)\
	API3X(SQL_API_SQLCLOSECURSOR)\
	ODBC_COLATTRIBUTE(API3X(SQL_API_SQLCOLATTRIBUTE))\
	API_X(SQL_API_SQLCOLATTRIBUTES)\
	API_X(SQL_API_SQLCOLUMNPRIVILEGES)\
	API_X(SQL_API_SQLCOLUMNS)\
	API_X(SQL_API_SQLCONNECT)\
	API3X(SQL_API_SQLCOPYDESC)\
	API_X(SQL_API_SQLDESCRIBECOL)\
	API_X(SQL_API_SQLDESCRIBEPARAM)\
	API_X(SQL_API_SQLDISCONNECT)\
	API_X(SQL_API_SQLDRIVERCONNECT)\
	API3X(SQL_API_SQLENDTRAN)\
	API_X(SQL_API_SQLERROR)\
	API_X(SQL_API_SQLEXECDIRECT)\
	API_X(SQL_API_SQLEXECUTE)\
	API_X(SQL_API_SQLEXTENDEDFETCH)\
	API_X(SQL_API_SQLFETCH)\
	API3X(SQL_API_SQLFETCHSCROLL)\
	API_X(SQL_API_SQLFOREIGNKEYS)\
	API_X(SQL_API_SQLFREECONNECT)\
	API_X(SQL_API_SQLFREEENV)\
	API3X(SQL_API_SQLFREEHANDLE)\
	API_X(SQL_API_SQLFREESTMT)\
	API3X(SQL_API_SQLGETCONNECTATTR)\
	API_X(SQL_API_SQLGETCONNECTOPTION)\
	API_X(SQL_API_SQLGETCURSORNAME)\
	API_X(SQL_API_SQLGETDATA)\
	API3X(SQL_API_SQLGETDESCFIELD)\
	API3X(SQL_API_SQLGETDESCREC)\
	API3X(SQL_API_SQLGETDIAGFIELD)\
	API3X(SQL_API_SQLGETDIAGREC)\
	API3X(SQL_API_SQLGETENVATTR)\
	API_X(SQL_API_SQLGETFUNCTIONS)\
	API_X(SQL_API_SQLGETINFO)\
	API3X(SQL_API_SQLGETSTMTATTR)\
	API_X(SQL_API_SQLGETSTMTOPTION)\
	API_X(SQL_API_SQLGETTYPEINFO)\
	API_X(SQL_API_SQLMORERESULTS)\
	API_X(SQL_API_SQLNATIVESQL)\
	API_X(SQL_API_SQLNUMPARAMS)\
	API_X(SQL_API_SQLNUMRESULTCOLS)\
	API_X(SQL_API_SQLPARAMDATA)\
	API_X(SQL_API_SQLPARAMOPTIONS)\
	API_X(SQL_API_SQLPREPARE)\
	API_X(SQL_API_SQLPRIMARYKEYS)\
	API_X(SQL_API_SQLPROCEDURECOLUMNS)\
	API_X(SQL_API_SQLPROCEDURES)\
	API_X(SQL_API_SQLPUTDATA)\
	API_X(SQL_API_SQLROWCOUNT)\
	API3X(SQL_API_SQLSETCONNECTATTR)\
	API_X(SQL_API_SQLSETCONNECTOPTION)\
	API_X(SQL_API_SQLSETCURSORNAME)\
	API3X(SQL_API_SQLSETDESCFIELD)\
	API3X(SQL_API_SQLSETDESCREC)\
	API3X(SQL_API_SQLSETENVATTR)\
	API_X(SQL_API_SQLSETPARAM)\
	API_X(SQL_API_SQLSETPOS)\
	API_X(SQL_API_SQLSETSCROLLOPTIONS)\
	API3X(SQL_API_SQLSETSTMTATTR)\
	API_X(SQL_API_SQLSETSTMTOPTION)\
	API_X(SQL_API_SQLSPECIALCOLUMNS)\
	API_X(SQL_API_SQLSTATISTICS)\
	API_X(SQL_API_SQLTABLEPRIVILEGES)\
	API_X(SQL_API_SQLTABLES)\
	API_X(SQL_API_SQLTRANSACT) \
	do {} while(0)

#define API_X(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define API__(n)
#define API3X(n) if (n >= 0 && n < (16*SQL_API_ODBC3_ALL_FUNCTIONS_SIZE)) pfExists[n/16] |= (1 << n%16);
#define API3_(n)
		ODBC_ALL_API;
#undef API_X
#undef API__
#undef API3X
#undef API3_
		break;
#endif

	case SQL_API_ALL_FUNCTIONS:
		tdsdump_log(TDS_DBG_FUNC, "SQLGetFunctions: " "fFunction is SQL_API_ALL_FUNCTIONS\n");
		for (i = 0; i < 100; ++i) {
			pfExists[i] = SQL_FALSE;
		}

#define API_X(n) if (n >= 0 && n < 100) pfExists[n] = SQL_TRUE;
#define API__(n)
#define API3X(n)
#define API3_(n)
		ODBC_ALL_API;
#undef API_X
#undef API__
#undef API3X
#undef API3_
		break;
#define API_X(n) case n:
#define API__(n)
#if (ODBCVER >= 0x300)
#define API3X(n) case n:
#else
#define API3X(n)
#endif
#define API3_(n)
		ODBC_ALL_API;
#undef API_X
#undef API__
#undef API3X
#undef API3_
		*pfExists = SQL_TRUE;
		break;
	default:
		*pfExists = SQL_FALSE;
		break;
	}
	ODBC_EXIT(dbc, SQL_SUCCESS);
#undef ODBC_ALL_API
}


SQLRETURN
odbc_SQLGetInfo(TDS_DBC *dbc, SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue, SQLSMALLINT cbInfoValueMax,
		SQLSMALLINT FAR *pcbInfoValue _WIDE)
{
	const char *p = NULL;
	char buf[32];
	TDSSOCKET *tds;
	int is_ms = -1;
	unsigned int smajor = 6;
	SQLUINTEGER mssql7plus_mask = 0;
	int out_len = -1;

	tdsdump_log(TDS_DBG_FUNC, "odbc_SQLGetInfo(%p, %u, %p, %d, %p)\n",
		    dbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue);

#define SIVAL out_len = sizeof(SQLSMALLINT), *((SQLSMALLINT *) rgbInfoValue)
#define USIVAL out_len = sizeof(SQLUSMALLINT), *((SQLUSMALLINT *) rgbInfoValue)
#define IVAL out_len = sizeof(SQLINTEGER), *((SQLINTEGER *) rgbInfoValue)
#define UIVAL out_len = sizeof(SQLUINTEGER), *((SQLUINTEGER *) rgbInfoValue)
#define ULVAL out_len = sizeof(SQLULEN), *((SQLULEN *) rgbInfoValue)

	if ((tds = dbc->tds_socket) != NULL) {
		is_ms = TDS_IS_MSSQL(tds);
		smajor = (tds->conn->product_version >> 24) & 0x7F;
		if (is_ms && smajor >= 7)
			mssql7plus_mask = ~((SQLUINTEGER) 0);
	}

	switch (fInfoType) {
	case SQL_ACCESSIBLE_PROCEDURES:
	case SQL_ACCESSIBLE_TABLES:
		p = "Y";
		break;
		/* SQL_ACTIVE_CONNECTIONS renamed to SQL_MAX_DRIVER_CONNECTIONS */
#if (ODBCVER >= 0x0300)
	case SQL_ACTIVE_ENVIRONMENTS:
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
#if (ODBCVER >= 0x0300)
	case SQL_AGGREGATE_FUNCTIONS:
		UIVAL = SQL_AF_ALL;
		break;
	case SQL_ALTER_DOMAIN:
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_ALTER_TABLE:
		UIVAL = SQL_AT_ADD_COLUMN | SQL_AT_ADD_COLUMN_DEFAULT | SQL_AT_ADD_COLUMN_SINGLE | SQL_AT_ADD_CONSTRAINT |
			SQL_AT_ADD_TABLE_CONSTRAINT | SQL_AT_CONSTRAINT_NAME_DEFINITION | SQL_AT_DROP_COLUMN_RESTRICT;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_ASYNC_MODE:
		/* TODO we hope so in a not-too-far future */
		/* UIVAL = SQL_AM_STATEMENT; */
		UIVAL = SQL_AM_NONE;
		break;
	case SQL_BATCH_ROW_COUNT:
		UIVAL = SQL_BRC_EXPLICIT;
		break;
	case SQL_BATCH_SUPPORT:
		UIVAL = SQL_BS_ROW_COUNT_EXPLICIT | SQL_BS_ROW_COUNT_PROC | SQL_BS_SELECT_EXPLICIT | SQL_BS_SELECT_PROC;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_BOOKMARK_PERSISTENCE:
		/* TODO ??? */
		UIVAL = SQL_BP_DELETE | SQL_BP_SCROLL | SQL_BP_UPDATE;
		break;
	case SQL_CATALOG_LOCATION:
		SIVAL = SQL_CL_START;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_CATALOG_NAME:
		p = "Y";
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_CATALOG_NAME_SEPARATOR:
		p = ".";
		break;
	case SQL_CATALOG_TERM:
		p = "database";
		break;
	case SQL_CATALOG_USAGE:
		UIVAL = SQL_CU_DML_STATEMENTS | SQL_CU_PROCEDURE_INVOCATION | SQL_CU_TABLE_DEFINITION;
		break;
		/* TODO */
#if 0
	case SQL_COLLATION_SEQ:
		break;
#endif
	case SQL_COLUMN_ALIAS:
		p = "Y";
		break;
	case SQL_CONCAT_NULL_BEHAVIOR:
		if (is_ms == -1)
			return SQL_ERROR;
		/* TODO a bit more complicate for mssql2k.. */
		SIVAL = (!is_ms || smajor < 7) ? SQL_CB_NON_NULL : SQL_CB_NULL;
		break;
		/* TODO SQL_CONVERT_BIGINT SQL_CONVERT_GUID SQL_CONVERT_DATE SQL_CONVERT_TIME */
		/* For Sybase/MSSql6.x we return 0 cause NativeSql is not so implemented */
	case SQL_CONVERT_BINARY:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_VARCHAR |
			 SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_TINYINT | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_BIT:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_CHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT |
			 SQL_CVT_TINYINT | SQL_CVT_TIMESTAMP | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_DECIMAL:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_FLOAT:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BIT | SQL_CVT_TINYINT | SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) &
			mssql7plus_mask;
		break;
	case SQL_CONVERT_INTEGER:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_LONGVARCHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_NUMERIC:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_REAL:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BIT | SQL_CVT_TINYINT | SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) &
			mssql7plus_mask;
		break;
	case SQL_CONVERT_SMALLINT:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_TIMESTAMP:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_TIMESTAMP | SQL_CVT_WCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_TINYINT:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT | SQL_CVT_TINYINT |
			 SQL_CVT_WCHAR | SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_VARBINARY:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_VARCHAR |
			 SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_TINYINT | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_VARCHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT |
			 SQL_CVT_TINYINT | SQL_CVT_TIMESTAMP | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_LONGVARBINARY:
		UIVAL = (SQL_CVT_BINARY | SQL_CVT_LONGVARBINARY | SQL_CVT_VARBINARY) & mssql7plus_mask;
		break;
	case SQL_CONVERT_WLONGVARCHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_CONVERT_WCHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT |
			 SQL_CVT_TINYINT | SQL_CVT_TIMESTAMP | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
	case SQL_CONVERT_WVARCHAR:
		UIVAL = (SQL_CVT_CHAR | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT |
			 SQL_CVT_REAL | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_BIT |
			 SQL_CVT_TINYINT | SQL_CVT_TIMESTAMP | SQL_CVT_LONGVARBINARY | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR |
			 SQL_CVT_WVARCHAR) & mssql7plus_mask;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_CONVERT_FUNCTIONS:
		/* TODO no CAST for Sybase ?? */
		UIVAL = SQL_FN_CVT_CONVERT | SQL_FN_CVT_CAST;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_CREATE_ASSERTION:
	case SQL_CREATE_CHARACTER_SET:
	case SQL_CREATE_COLLATION:
	case SQL_CREATE_DOMAIN:
		UIVAL = 0;
		break;
	case SQL_CREATE_SCHEMA:
		UIVAL = SQL_CS_AUTHORIZATION | SQL_CS_CREATE_SCHEMA;
		break;
	case SQL_CREATE_TABLE:
		UIVAL = SQL_CT_CREATE_TABLE;
		break;
	case SQL_CREATE_TRANSLATION:
		UIVAL = 0;
		break;
	case SQL_CREATE_VIEW:
		UIVAL = SQL_CV_CHECK_OPTION | SQL_CV_CREATE_VIEW;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_CORRELATION_NAME:
		USIVAL = SQL_CN_ANY;
		break;
	case SQL_CURSOR_COMMIT_BEHAVIOR:
		/* currently cursors are not supported however sql server close automaticly cursors on commit */
		/* TODO cursors test what happen if rollback, cursors get properly deleted ?? */
		USIVAL = SQL_CB_CLOSE;
		break;
	case SQL_CURSOR_ROLLBACK_BEHAVIOR:
		USIVAL = SQL_CB_CLOSE;
		break;
	case SQL_CURSOR_SENSITIVITY:
		UIVAL = SQL_SENSITIVE;
		break;
	case SQL_DATABASE_NAME:
		p = tds_dstr_cstr(&dbc->attr.current_catalog);
		break;
	case SQL_DATA_SOURCE_NAME:
		p = tds_dstr_cstr(&dbc->dsn);
		break;
	case SQL_DATA_SOURCE_READ_ONLY:
		/*
		 * TODO: determine the right answer from connection
		 * attribute SQL_ATTR_ACCESS_MODE
		 */
		p = "N";	/* false, writable */
		break;
#if (ODBCVER >= 0x0300)
	case SQL_DATETIME_LITERALS:
		/* TODO ok ?? */
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_DBMS_NAME:
		p = tds ? tds->conn->product_name : NULL;
		break;
	case SQL_DBMS_VER:
		if (!dbc->tds_socket)
			return SQL_ERROR;
		odbc_rdbms_version(dbc->tds_socket, buf);
		p = buf;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_DDL_INDEX:
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_DEFAULT_TXN_ISOLATION:
		UIVAL = SQL_TXN_READ_COMMITTED;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_DESCRIBE_PARAMETER:
		p = "Y";
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_DRIVER_HDBC:
		ULVAL = (SQLULEN) dbc;
		break;
	case SQL_DRIVER_HENV:
		ULVAL = (SQLULEN) dbc->env;
		break;
	case SQL_DRIVER_HSTMT:{
			void *ptr = *((void **) rgbInfoValue);

			if (SQL_NULL_HSTMT != ptr && !IS_HSTMT(ptr))
				ULVAL = SQL_NULL_HSTMT;
		}
		break;
	case SQL_DRIVER_HDESC:{
			void *ptr = *((void **) rgbInfoValue);

			if (SQL_NULL_HDESC != ptr && !IS_HDESC(ptr))
				ULVAL = SQL_NULL_HDESC;
		}
		break;
	case SQL_DRIVER_NAME:	/* ODBC 2.0 */
		p = "libtdsodbc.so";
		break;
	case SQL_DRIVER_ODBC_VER:
		p = "03.50";
		break;
	case SQL_DRIVER_VER:
		sprintf(buf, "%02d.%02d.%04d", TDS_VERSION_MAJOR, TDS_VERSION_MINOR, TDS_VERSION_SUBVERSION);
		p = buf;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_DROP_ASSERTION:
	case SQL_DROP_CHARACTER_SET:
	case SQL_DROP_COLLATION:
	case SQL_DROP_DOMAIN:
	case SQL_DROP_SCHEMA:
		UIVAL = 0;
		break;
	case SQL_DROP_TABLE:
		UIVAL = SQL_DT_DROP_TABLE;
		break;
	case SQL_DROP_TRANSLATION:
		UIVAL = 0;
		break;
	case SQL_DROP_VIEW:
		UIVAL = SQL_DV_DROP_VIEW;
		break;
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
		if (dbc->cursor_support)
			/* TODO cursor SQL_CA1_BULK_ADD SQL_CA1_POS_REFRESH SQL_CA1_SELECT_FOR_UPDATE */
			UIVAL = SQL_CA1_ABSOLUTE | SQL_CA1_NEXT | SQL_CA1_RELATIVE |
				SQL_CA1_LOCK_NO_CHANGE |
				SQL_CA1_POS_DELETE | SQL_CA1_POS_POSITION | SQL_CA1_POS_UPDATE |
				SQL_CA1_POSITIONED_UPDATE | SQL_CA1_POSITIONED_DELETE;
		else
			UIVAL = 0;
		break;
	case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
		/* TODO cursors */
		/* Cursors not supported yet */
		/*
		 * Should be SQL_CA2_LOCK_CONCURRENCY SQL_CA2_MAX_ROWS_CATALOG SQL_CA2_MAX_ROWS_DELETE
		 * SQL_CA2_MAX_ROWS_INSERT SQL_CA2_MAX_ROWS_SELECT SQL_CA2_MAX_ROWS_UPDATE SQL_CA2_OPT_ROWVER_CONCURRENCY
		 * SQL_CA2_OPT_VALUES_CONCURRENCY SQL_CA2_READ_ONLY_CONCURRENCY SQL_CA2_SENSITIVITY_ADDITIONS
		 * SQL_CA2_SENSITIVITY_UPDATES SQL_CA2_SIMULATE_UNIQUE
		 */
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_EXPRESSIONS_IN_ORDERBY:
		p = "Y";
		break;
	case SQL_FILE_USAGE:
		USIVAL = SQL_FILE_NOT_SUPPORTED;
		break;
	case SQL_FETCH_DIRECTION:
		if (dbc->cursor_support)
			/* TODO cursors SQL_FD_FETCH_BOOKMARK */
			UIVAL = SQL_FD_FETCH_ABSOLUTE | SQL_FD_FETCH_FIRST | SQL_FD_FETCH_LAST | SQL_FD_FETCH_NEXT
				| SQL_FD_FETCH_PRIOR | SQL_FD_FETCH_RELATIVE;
		else
			UIVAL = 0;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
		if (dbc->cursor_support)
			/* TODO cursors SQL_CA1_SELECT_FOR_UPDATE */
			UIVAL = SQL_CA1_NEXT | SQL_CA1_POSITIONED_DELETE | SQL_CA1_POSITIONED_UPDATE;
		else
			UIVAL = 0;
		break;
	case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
		/* TODO cursors */
		/* Cursors not supported yet */
		/*
		 * Should be SQL_CA2_LOCK_CONCURRENCY SQL_CA2_MAX_ROWS_CATALOG SQL_CA2_MAX_ROWS_DELETE
		 * SQL_CA2_MAX_ROWS_INSERT SQL_CA2_MAX_ROWS_SELECT SQL_CA2_MAX_ROWS_UPDATE SQL_CA2_OPT_ROWVER_CONCURRENCY
		 * SQL_CA2_OPT_VALUES_CONCURRENCY SQL_CA2_READ_ONLY_CONCURRENCY
		 */
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_GETDATA_EXTENSIONS:
		/* TODO FreeTDS support more ?? */
		UIVAL = SQL_GD_BLOCK;
		break;
	case SQL_GROUP_BY:
		USIVAL = SQL_GB_GROUP_BY_CONTAINS_SELECT;
		break;
	case SQL_IDENTIFIER_CASE:
		/* TODO configuration dependend */
		USIVAL = SQL_IC_MIXED;
		break;
	case SQL_IDENTIFIER_QUOTE_CHAR:
		p = "\"";
		/* TODO workaround for Sybase, use SET QUOTED_IDENTIFIER ON and fix unwanted quote */
		if (!is_ms)
			p = "";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_INDEX_KEYWORDS:
		UIVAL = SQL_IK_ASC | SQL_IK_DESC;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_INFO_SCHEMA_VIEWS:
		/* TODO finish */
		UIVAL = 0;
		break;
	case SQL_INSERT_STATEMENT:
		UIVAL = 0;
		break;
		/* renamed from SQL_ODBC_SQL_OPT_IEF */
	case SQL_INTEGRITY:
		p = "Y";
		break;
	case SQL_KEYSET_CURSOR_ATTRIBUTES1:
	case SQL_KEYSET_CURSOR_ATTRIBUTES2:
		/* TODO cursors */
		UIVAL = 0;
		break;
	case SQL_KEYWORDS:
		/* TODO ok for Sybase ? */
		p = "BREAK,BROWSE,BULK,CHECKPOINT,CLUSTERED,"
			"COMMITTED,COMPUTE,CONFIRM,CONTROLROW,DATABASE,"
			"DBCC,DISK,DISTRIBUTED,DUMMY,DUMP,ERRLVL,"
			"ERROREXIT,EXIT,FILE,FILLFACTOR,FLOPPY,HOLDLOCK,"
			"IDENTITY_INSERT,IDENTITYCOL,IF,KILL,LINENO,"
			"LOAD,MIRROREXIT,NONCLUSTERED,OFF,OFFSETS,ONCE,"
			"OVER,PERCENT,PERM,PERMANENT,PLAN,PRINT,PROC,"
			"PROCESSEXIT,RAISERROR,READ,READTEXT,RECONFIGURE,"
			"REPEATABLE,RETURN,ROWCOUNT,RULE,SAVE,SERIALIZABLE,"
			"SETUSER,SHUTDOWN,STATISTICS,TAPE,TEMP,TEXTSIZE,"
			"TRAN,TRIGGER,TRUNCATE,TSEQUEL,UNCOMMITTED," "UPDATETEXT,USE,WAITFOR,WHILE,WRITETEXT";
		break;
	case SQL_LIKE_ESCAPE_CLAUSE:
		p = "Y";
		break;
	case SQL_LOCK_TYPES:
		if (dbc->cursor_support)
			IVAL = SQL_LCK_NO_CHANGE;
		else
			IVAL = 0;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
		UIVAL = 1;
		break;
#endif /* ODBCVER >= 0x0300 */
		/* TODO same limits for Sybase ? */
	case SQL_MAX_BINARY_LITERAL_LEN:
		UIVAL = 131072;
		break;
	case SQL_MAX_CHAR_LITERAL_LEN:
		UIVAL = 131072;
		break;
	case SQL_MAX_CONCURRENT_ACTIVITIES:
		USIVAL = 1;
		break;
	case SQL_MAX_COLUMNS_IN_GROUP_BY:
	case SQL_MAX_COLUMNS_IN_INDEX:
	case SQL_MAX_COLUMNS_IN_ORDER_BY:
		/* TODO Sybase ? */
		USIVAL = 16;
		break;
	case SQL_MAX_COLUMNS_IN_SELECT:
		/* TODO Sybase ? */
		USIVAL = 4000;
		break;
	case SQL_MAX_COLUMNS_IN_TABLE:
		/* TODO Sybase ? */
		USIVAL = 250;
		break;
	case SQL_MAX_DRIVER_CONNECTIONS:
		USIVAL = 0;
		break;
	case SQL_MAX_IDENTIFIER_LEN:
		if (is_ms == -1)
			return SQL_ERROR;
		USIVAL = (!is_ms || smajor < 7) ? 30 : 128;
		break;
	case SQL_MAX_INDEX_SIZE:
		UIVAL = 127;
		break;
	case SQL_MAX_PROCEDURE_NAME_LEN:
		if (is_ms == -1)
			return SQL_ERROR;
		/* TODO Sybase ? */
		USIVAL = (is_ms && smajor >= 7) ? 134 : 36;
		break;
	case SQL_MAX_ROW_SIZE:
		if (is_ms == -1)
			return SQL_ERROR;
		/* TODO Sybase ? */
		UIVAL = (is_ms && smajor >= 7) ? 8062 : 1962;
		break;
	case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
		p = "N";
		break;
	case SQL_MAX_STATEMENT_LEN:
		/* TODO Sybase ? */
		UIVAL = 131072;
		break;
	case SQL_MAX_TABLES_IN_SELECT:
		/* TODO Sybase ? */
		USIVAL = 16;
		break;
	case SQL_MAX_USER_NAME_LEN:
		/* TODO Sybase ? */
		if (is_ms == -1)
			return SQL_ERROR;
		USIVAL = (is_ms && smajor >= 7) ? 128 : 30;
		break;
	case SQL_MAX_COLUMN_NAME_LEN:
	case SQL_MAX_CURSOR_NAME_LEN:
	case SQL_MAX_SCHEMA_NAME_LEN:
	case SQL_MAX_CATALOG_NAME_LEN:
	case SQL_MAX_TABLE_NAME_LEN:
		/* TODO Sybase ? */
		if (is_ms == -1)
			return SQL_ERROR;
		USIVAL = (is_ms && smajor >= 7) ? 128 : 30;
		break;
	case SQL_MULT_RESULT_SETS:
		p = "Y";
		break;
	case SQL_MULTIPLE_ACTIVE_TXN:
		p = "Y";
		break;
	case SQL_NEED_LONG_DATA_LEN:
		/* current implementation do not require length, however future will, so is correct to return yes */
		p = "Y";
		break;
	case SQL_NON_NULLABLE_COLUMNS:
		USIVAL = SQL_NNC_NON_NULL;
		break;
	case SQL_NULL_COLLATION:
		USIVAL = SQL_NC_LOW;
		break;
	case SQL_NUMERIC_FUNCTIONS:
		UIVAL = (SQL_FN_NUM_ABS | SQL_FN_NUM_ACOS | SQL_FN_NUM_ASIN | SQL_FN_NUM_ATAN | SQL_FN_NUM_ATAN2 |
			 SQL_FN_NUM_CEILING | SQL_FN_NUM_COS | SQL_FN_NUM_COT | SQL_FN_NUM_DEGREES | SQL_FN_NUM_EXP |
			 SQL_FN_NUM_FLOOR | SQL_FN_NUM_LOG | SQL_FN_NUM_LOG10 | SQL_FN_NUM_MOD | SQL_FN_NUM_PI | SQL_FN_NUM_POWER |
			 SQL_FN_NUM_RADIANS | SQL_FN_NUM_RAND | SQL_FN_NUM_ROUND | SQL_FN_NUM_SIGN | SQL_FN_NUM_SIN |
			 SQL_FN_NUM_SQRT | SQL_FN_NUM_TAN) & mssql7plus_mask;
		break;
	case SQL_ODBC_API_CONFORMANCE:
		SIVAL = SQL_OAC_LEVEL2;
		break;
#if (ODBCVER >= 0x0300)
	case SQL_ODBC_INTERFACE_CONFORMANCE:
		UIVAL = SQL_OAC_LEVEL2;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_ODBC_SAG_CLI_CONFORMANCE:
		SIVAL = SQL_OSCC_NOT_COMPLIANT;
		break;
	case SQL_ODBC_SQL_CONFORMANCE:
		SIVAL = SQL_OSC_CORE;
		break;
#ifdef TDS_NO_DM
	case SQL_ODBC_VER:
		/* TODO check format ##.##.0000 */
		p = VERSION;
		break;
#endif
#if (ODBCVER >= 0x0300)
	case SQL_OJ_CAPABILITIES:
		UIVAL = SQL_OJ_ALL_COMPARISON_OPS | SQL_OJ_FULL | SQL_OJ_INNER | SQL_OJ_LEFT | SQL_OJ_NESTED | SQL_OJ_NOT_ORDERED |
			SQL_OJ_RIGHT;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_ORDER_BY_COLUMNS_IN_SELECT:
		p = "N";
		break;
	case SQL_OUTER_JOINS:
		p = "Y";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_PARAM_ARRAY_ROW_COUNTS:
		UIVAL = SQL_PARC_BATCH;
		break;
	case SQL_PARAM_ARRAY_SELECTS:
		UIVAL = SQL_PAS_BATCH;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_POS_OPERATIONS:
		if (dbc->cursor_support)
			/* TODO cursors SQL_POS_ADD SQL_POS_REFRESH */
			/* test REFRESH, ADD under mssql, under Sybase I don't know how to do it but dblib do */
			IVAL = SQL_POS_DELETE | SQL_POS_POSITION | SQL_POS_UPDATE;
		else
			IVAL = 0;
		break;
	case SQL_POSITIONED_STATEMENTS:
		/* TODO cursors SQL_PS_SELECT_FOR_UPDATE */
		IVAL = SQL_PS_POSITIONED_DELETE | SQL_PS_POSITIONED_UPDATE;
		break;
	case SQL_PROCEDURE_TERM:
		p = "stored procedure";
		break;
	case SQL_PROCEDURES:
		p = "Y";
		break;
	case SQL_QUOTED_IDENTIFIER_CASE:
		/* TODO usually insensitive */
		USIVAL = SQL_IC_MIXED;
		break;
	case SQL_ROW_UPDATES:
		p = "N";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_SCHEMA_TERM:
		p = "owner";
		break;
	case SQL_SCHEMA_USAGE:
		UIVAL = SQL_OU_DML_STATEMENTS | SQL_OU_INDEX_DEFINITION | SQL_OU_PRIVILEGE_DEFINITION | SQL_OU_PROCEDURE_INVOCATION
			| SQL_OU_TABLE_DEFINITION;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_SCROLL_CONCURRENCY:
		if (dbc->cursor_support)
			/* TODO cursors test them, Sybase support all ?  */
			IVAL = SQL_SCCO_LOCK | SQL_SCCO_OPT_ROWVER | SQL_SCCO_OPT_VALUES | SQL_SCCO_READ_ONLY;
		else
			IVAL = SQL_SCCO_READ_ONLY;
		break;
	case SQL_SCROLL_OPTIONS:
		if (dbc->cursor_support)
			/* TODO cursors test them, Sybase support all ?? */
			UIVAL = SQL_SO_DYNAMIC | SQL_SO_FORWARD_ONLY | SQL_SO_KEYSET_DRIVEN | SQL_SO_STATIC;
		else
			UIVAL = SQL_SO_FORWARD_ONLY | SQL_SO_STATIC;
		break;
	case SQL_SEARCH_PATTERN_ESCAPE:
		p = "\\";
		break;
	case SQL_SERVER_NAME:
		p = dbc->tds_socket->conn->server;
		break;
	case SQL_SPECIAL_CHARACTERS:
		/* TODO others ?? */
		p = "\'\"[]{}";
		break;
#if (ODBCVER >= 0x0300)
	case SQL_SQL_CONFORMANCE:
		UIVAL = SQL_SC_SQL92_ENTRY;
		break;
	case SQL_SQL92_DATETIME_FUNCTIONS:
	case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:
	case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
		UIVAL = 0;
		break;
	case SQL_SQL92_GRANT:
		UIVAL = SQL_SG_WITH_GRANT_OPTION;
		break;
	case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
		UIVAL = 0;
		break;
	case SQL_SQL92_PREDICATES:
		/* TODO Sybase ?? */
		UIVAL = SQL_SP_EXISTS | SQL_SP_ISNOTNULL | SQL_SP_ISNULL;
		break;
	case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
		/* TODO Sybase ?? */
		UIVAL = SQL_SRJO_CROSS_JOIN | SQL_SRJO_FULL_OUTER_JOIN | SQL_SRJO_INNER_JOIN | SQL_SRJO_LEFT_OUTER_JOIN |
			SQL_SRJO_RIGHT_OUTER_JOIN | SQL_SRJO_UNION_JOIN;
		break;
	case SQL_SQL92_REVOKE:
		UIVAL = SQL_SR_GRANT_OPTION_FOR;
		break;
	case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:
		UIVAL = SQL_SRVC_DEFAULT | SQL_SRVC_NULL | SQL_SRVC_ROW_SUBQUERY | SQL_SRVC_VALUE_EXPRESSION;
		break;
	case SQL_SQL92_STRING_FUNCTIONS:
		UIVAL = SQL_SSF_LOWER | SQL_SSF_UPPER;
		break;
	case SQL_SQL92_VALUE_EXPRESSIONS:
		/* TODO Sybase ? CAST supported ? */
		UIVAL = SQL_SVE_CASE | SQL_SVE_CAST | SQL_SVE_COALESCE | SQL_SVE_NULLIF;
		break;
	case SQL_STANDARD_CLI_CONFORMANCE:
		UIVAL = SQL_SCC_ISO92_CLI;
		break;
	case SQL_STATIC_SENSITIVITY:
		IVAL = 0;
		break;
	case SQL_STATIC_CURSOR_ATTRIBUTES1:
		if (dbc->cursor_support)
			/* TODO cursors SQL_CA1_BOOKMARK SQL_CA1_BULK_FETCH_BY_BOOKMARK SQL_CA1_POS_REFRESH */
			UIVAL = SQL_CA1_ABSOLUTE | SQL_CA1_LOCK_NO_CHANGE | SQL_CA1_NEXT | SQL_CA1_POS_POSITION | SQL_CA1_RELATIVE;
		else
			UIVAL = 0;
		break;
	case SQL_STATIC_CURSOR_ATTRIBUTES2:
		/* TODO cursors */
		UIVAL = 0;
		break;
#endif /* ODBCVER >= 0x0300 */
	case SQL_STRING_FUNCTIONS:
		UIVAL = (SQL_FN_STR_ASCII | SQL_FN_STR_BIT_LENGTH | SQL_FN_STR_CHAR | SQL_FN_STR_CONCAT | SQL_FN_STR_DIFFERENCE |
			 SQL_FN_STR_INSERT | SQL_FN_STR_LCASE | SQL_FN_STR_LEFT | SQL_FN_STR_LENGTH | SQL_FN_STR_LOCATE_2 |
			 SQL_FN_STR_LTRIM | SQL_FN_STR_OCTET_LENGTH | SQL_FN_STR_REPEAT | SQL_FN_STR_RIGHT | SQL_FN_STR_RTRIM |
			 SQL_FN_STR_SOUNDEX | SQL_FN_STR_SPACE | SQL_FN_STR_SUBSTRING | SQL_FN_STR_UCASE) & mssql7plus_mask;
		break;
	case SQL_SUBQUERIES:
		UIVAL = SQL_SQ_COMPARISON | SQL_SQ_CORRELATED_SUBQUERIES | SQL_SQ_EXISTS | SQL_SQ_IN | SQL_SQ_QUANTIFIED;
		break;
	case SQL_SYSTEM_FUNCTIONS:
		UIVAL = SQL_FN_SYS_DBNAME | SQL_FN_SYS_IFNULL | SQL_FN_SYS_USERNAME;
		break;
	case SQL_TABLE_TERM:
		p = "table";
		break;
	case SQL_TIMEDATE_ADD_INTERVALS:
		UIVAL = (SQL_FN_TSI_DAY | SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_HOUR | SQL_FN_TSI_MINUTE | SQL_FN_TSI_MONTH |
			 SQL_FN_TSI_QUARTER | SQL_FN_TSI_SECOND | SQL_FN_TSI_WEEK | SQL_FN_TSI_YEAR) & mssql7plus_mask;
		break;
	case SQL_TIMEDATE_DIFF_INTERVALS:
		UIVAL = (SQL_FN_TSI_DAY | SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_HOUR | SQL_FN_TSI_MINUTE | SQL_FN_TSI_MONTH |
			 SQL_FN_TSI_QUARTER | SQL_FN_TSI_SECOND | SQL_FN_TSI_WEEK | SQL_FN_TSI_YEAR) & mssql7plus_mask;
		break;
	case SQL_TIMEDATE_FUNCTIONS:
		UIVAL = (SQL_FN_TD_CURDATE | SQL_FN_TD_CURRENT_DATE | SQL_FN_TD_CURRENT_TIME | SQL_FN_TD_CURRENT_TIMESTAMP |
			 SQL_FN_TD_CURTIME | SQL_FN_TD_DAYNAME | SQL_FN_TD_DAYOFMONTH | SQL_FN_TD_DAYOFWEEK | SQL_FN_TD_DAYOFYEAR |
			 SQL_FN_TD_EXTRACT | SQL_FN_TD_HOUR | SQL_FN_TD_MINUTE | SQL_FN_TD_MONTH | SQL_FN_TD_MONTHNAME |
			 SQL_FN_TD_NOW | SQL_FN_TD_QUARTER | SQL_FN_TD_SECOND | SQL_FN_TD_TIMESTAMPADD | SQL_FN_TD_TIMESTAMPDIFF |
			 SQL_FN_TD_WEEK | SQL_FN_TD_YEAR) & mssql7plus_mask;
		break;
	case SQL_TXN_CAPABLE:
		/* transaction for DML and DDL */
		SIVAL = SQL_TC_ALL;
		break;
	case SQL_TXN_ISOLATION_OPTION:
		UIVAL = SQL_TXN_READ_COMMITTED | SQL_TXN_READ_UNCOMMITTED | SQL_TXN_REPEATABLE_READ | SQL_TXN_SERIALIZABLE;
		break;
	case SQL_UNION:
		UIVAL = SQL_U_UNION | SQL_U_UNION_ALL;
		break;
		/* TODO finish */
	case SQL_USER_NAME:
		/* TODO this is not correct username */
		/* p = tds_dstr_cstr(&dbc->tds_login->user_name); */
		/* make OpenOffice happy :) */
		p = "";
		break;
	case SQL_XOPEN_CLI_YEAR:
		/* TODO check specifications */
		p = "1995";
		break;
	case SQL_INFO_FREETDS_TDS_VERSION:
		if (!dbc->tds_socket)
			return SQL_ERROR;
		UIVAL = TDS_MAJOR(dbc->tds_socket->conn) << 16 | TDS_MINOR(dbc->tds_socket->conn);
		break;
	case SQL_INFO_FREETDS_SOCKET:
		if (IS_TDSDEAD(dbc->tds_socket))
			return SQL_ERROR;
		ULVAL = dbc->tds_socket->conn->s;
		break;
	default:
		odbc_log_unimplemented_type("SQLGetInfo", fInfoType);
		odbc_errs_add(&dbc->errs, "HY092", "Option not supported");
		return SQL_ERROR;
	}

	/* char data */
	if (p) {
		return odbc_set_string_oct(dbc, rgbInfoValue, cbInfoValueMax, pcbInfoValue, p, -1);
	} else {
		if (out_len > 0 && pcbInfoValue)
			*pcbInfoValue = out_len;
	}

	return SQL_SUCCESS;
#undef SIVAL
#undef USIVAL
#undef IVAL
#undef UIVAL
}

SQLRETURN ODBC_PUBLIC ODBC_API
SQLGetInfo(SQLHDBC hdbc, SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue, SQLSMALLINT cbInfoValueMax, SQLSMALLINT FAR *pcbInfoValue)
{
	ODBC_ENTER_HDBC;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetInfo(%p, %d, %p, %d, %p)\n", hdbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue);

	ODBC_EXIT(dbc, odbc_SQLGetInfo(dbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue _wide0));
}

#ifdef ENABLE_ODBC_WIDE
SQLRETURN ODBC_PUBLIC ODBC_API
SQLGetInfoW(SQLHDBC hdbc, SQLUSMALLINT fInfoType, SQLPOINTER rgbInfoValue, SQLSMALLINT cbInfoValueMax,
	    SQLSMALLINT FAR *pcbInfoValue)
{
	ODBC_ENTER_HDBC;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetInfoW(%p, %d, %p, %d, %p)\n", hdbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue);

	ODBC_EXIT(dbc, odbc_SQLGetInfo(dbc, fInfoType, rgbInfoValue, cbInfoValueMax, pcbInfoValue, 1));
}
#endif
