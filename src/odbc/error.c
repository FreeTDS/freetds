/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2003  Frediano Ziglio
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
#include "tdsstring.h"
#include "tdsodbc.h"
#include "odbc_util.h"

#if HAVE_ODBCSS_H
#include <odbcss.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: error.c,v 1.20 2003-07-27 12:08:57 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void sqlstate2to3(char *state);
static void odbc_errs_pop(struct _sql_errors *errs);

#define ODBCERR(s2,s3,msg) { msg, s2, s3 }
static const struct _sql_error_struct odbc_errs[] = {
	ODBCERR("S1000", "HY000", "General driver error"),
	ODBCERR("S1C00", "HYC00", "Optional feature not implemented"),
	ODBCERR("S1001", "HY001", "Memory allocation error"),
	/* TODO find best errors for ODBC version 2 */
	ODBCERR("S1000", "IM007", "No data source or driver specified"),
	ODBCERR("S1000", "08001", "Client unable to establish connection"),
	ODBCERR("S1002", "07009", "Invalid index"),
	ODBCERR("S1003", "HY004", "Invalid data type"),
	ODBCERR("S1090", "HY090", "Invalid buffer length"),
	ODBCERR("01004", "01004", "Data truncation"),
	ODBCERR("S1010", "07005", "No result available"),
	ODBCERR("S1092", "HY092", "Invalid option")
};

void
odbc_errs_reset(struct _sql_errors *errs)
{
	int i;

	if (errs->errs) {
		for (i = 0; i < errs->num_errors; ++i) {
			if (errs->errs[i].msg)
				free(errs->errs[i].msg);
		}
		free(errs->errs);
		errs->errs = NULL;
	}
	errs->num_errors = 0;
}

/** Remove first element */
static void
odbc_errs_pop(struct _sql_errors *errs)
{
	if (!errs || !errs->errs || errs->num_errors <= 0)
		return;

	if (errs->num_errors == 1) {
		odbc_errs_reset(errs);
		return;
	}

	if (errs->errs[0].msg)
		free(errs->errs[0].msg);

	--errs->num_errors;
	memmove(&(errs->errs[0]), &(errs->errs[1]), errs->num_errors * sizeof(errs->errs[0]));
}

void
odbc_errs_add_rdbms(struct _sql_errors *errs, enum _sql_error_types err_type, char *msg, char *sqlstate, int msgnum,
		    unsigned short linenum, int msgstate)
{
	struct _sql_error *p;
	int n = errs->num_errors;

	if (errs->errs)
		p = (struct _sql_error *) realloc(errs->errs, sizeof(struct _sql_error) * (n + 1));
	else
		p = (struct _sql_error *) malloc(sizeof(struct _sql_error));
	if (!p)
		return;

	errs->errs = p;
	errs->errs[n].err = &odbc_errs[err_type];
	errs->errs[n].msg = msg ? strdup(msg) : NULL;
	if (sqlstate != NULL) {
		strncpy(errs->errs[n].sqlstate, sqlstate, 5);
		errs->errs[n].sqlstate[5] = '\0';
	} else
		errs->errs[n].sqlstate[0] = '\0';
	errs->errs[n].msgnum = msgnum;
	errs->errs[n].linenum = linenum;
	errs->errs[n].msgstate = msgstate;
	++errs->num_errors;
}

void
odbc_errs_add(struct _sql_errors *errs, enum _sql_error_types err_type, const char *msg)
{
	struct _sql_error *p;
	int n = errs->num_errors;

	if (errs->errs)
		p = (struct _sql_error *) realloc(errs->errs, sizeof(struct _sql_error) * (n + 1));
	else
		p = (struct _sql_error *) malloc(sizeof(struct _sql_error));
	if (!p)
		return;

	errs->errs = p;
	errs->errs[n].err = &odbc_errs[err_type];
	errs->errs[n].msg = msg ? strdup(msg) : NULL;
	errs->errs[n].sqlstate[0] = '\0';
	errs->errs[n].msgnum = 0;
	errs->errs[n].msgstate = 0;
	errs->errs[n].linenum = 0;
	++errs->num_errors;
}

#define SQLS_MAP(v2,v3) if (strcmp(p,v2) == 0) {strcpy(p,v3); return;}
static void
sqlstate2to3(char *state)
{
	char *p = state;

	if (p[0] == 'S' && p[1] == '0' && p[2] == '0') {
		p[0] = '4';
		p[1] = '2';
		p[2] = 'S';
		return;
	}

	/* TODO optimize with a switch */
	SQLS_MAP("01S03", "01001");
	SQLS_MAP("01S04", "01001");
	SQLS_MAP("22003", "HY019");
	SQLS_MAP("22008", "22007");
	SQLS_MAP("22005", "22018");
	SQLS_MAP("24000", "07005");
	SQLS_MAP("37000", "42000");
	SQLS_MAP("70100", "HY018");
	SQLS_MAP("S1000", "HY000");
	SQLS_MAP("S1001", "HY001");
	SQLS_MAP("S1002", "07009");
	SQLS_MAP("S1003", "HY003");
	SQLS_MAP("S1004", "HY004");
	SQLS_MAP("S1008", "HY008");
	SQLS_MAP("S1009", "HY009");
	SQLS_MAP("S1010", "HY007");
	SQLS_MAP("S1011", "HY011");
	SQLS_MAP("S1012", "HY012");
	SQLS_MAP("S1090", "HY090");
	SQLS_MAP("S1091", "HY091");
	SQLS_MAP("S1092", "HY092");
	SQLS_MAP("S1093", "07009");
	SQLS_MAP("S1096", "HY096");
	SQLS_MAP("S1097", "HY097");
	SQLS_MAP("S1098", "HY098");
	SQLS_MAP("S1099", "HY099");
	SQLS_MAP("S1100", "HY100");
	SQLS_MAP("S1101", "HY101");
	SQLS_MAP("S1103", "HY103");
	SQLS_MAP("S1104", "HY104");
	SQLS_MAP("S1105", "HY105");
	SQLS_MAP("S1106", "HY106");
	SQLS_MAP("S1107", "HY107");
	SQLS_MAP("S1108", "HY108");
	SQLS_MAP("S1109", "HY109");
	SQLS_MAP("S1110", "HY110");
	SQLS_MAP("S1111", "HY111");
	SQLS_MAP("S1C00", "HYC00");
	SQLS_MAP("S1T00", "HYT00");
}


static SQLRETURN
_SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLCHAR FAR * szSqlState,
	       SQLINTEGER FAR * pfNativeError, SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	SQLRETURN result;
	struct _sql_errors *errs = NULL;
	const char *msg;
	unsigned char odbc_ver = SQL_OV_ODBC2;
	TDS_STMT *stmt = NULL;
	TDS_DBC *dbc = NULL;
	TDS_ENV *env = NULL;

	if (numRecord <= 0 || cbErrorMsgMax < 0 || !handle)
		return SQL_ERROR;

	switch (handleType) {
	case SQL_HANDLE_STMT:
		stmt = (TDS_STMT *) handle;
		dbc = stmt->hdbc;
		env = dbc->henv;
		errs = &stmt->errs;
		break;

	case SQL_HANDLE_DBC:
		dbc = ((TDS_DBC *) handle);
		env = dbc->henv;
		errs = &dbc->errs;
		break;

	case SQL_HANDLE_ENV:
		env = ((TDS_ENV *) handle);
		errs = &env->errs;
		break;

	default:
		return SQL_INVALID_HANDLE;
	}
	odbc_ver = env->attr.attr_odbc_version;

	if (numRecord > errs->num_errors)
		return SQL_NO_DATA_FOUND;
	--numRecord;

	if (szSqlState) {
		if (*errs->errs[numRecord].sqlstate != '\0') {
			strcpy(szSqlState, errs->errs[numRecord].sqlstate);
			if (odbc_ver == SQL_OV_ODBC3)
				sqlstate2to3(szSqlState);
		} else if (odbc_ver == SQL_OV_ODBC3)
			strcpy((char *) szSqlState, errs->errs[numRecord].err->state3);
		else
			strcpy((char *) szSqlState, errs->errs[numRecord].err->state2);
	}

	msg = errs->errs[numRecord].msg;
	if (!msg)
		msg = errs->errs[numRecord].err->msg;
	result = odbc_set_string(szErrorMsg, cbErrorMsgMax, pcbErrorMsg, msg, -1);
	if (pfNativeError)
		*pfNativeError = errs->errs[numRecord].msgnum;

	return result;
}

SQLRETURN SQL_API
SQLError(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt, SQLCHAR FAR * szSqlState, SQLINTEGER FAR * pfNativeError,
	 SQLCHAR FAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	SQLRETURN result;
	struct _sql_errors *errs = NULL;
	SQLSMALLINT type;
	SQLHANDLE handle;

	if (hstmt) {
		errs = &((TDS_STMT *) hstmt)->errs;
		handle = hstmt;
		type = SQL_HANDLE_STMT;
	} else if (hdbc) {
		errs = &((TDS_DBC *) hdbc)->errs;
		handle = hdbc;
		type = SQL_HANDLE_DBC;
	} else if (henv) {
		errs = &((TDS_ENV *) henv)->errs;
		handle = henv;
		type = SQL_HANDLE_ENV;
	} else
		return SQL_INVALID_HANDLE;


	result = _SQLGetDiagRec(type, handle, 1, szSqlState, pfNativeError, szErrorMsg, cbErrorMsgMax, pcbErrorMsg);

	if (result == SQL_SUCCESS) {
		/* remove first error */
		odbc_errs_pop(errs);
	}

	return result;
}

#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API
SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLCHAR FAR * szSqlState,
	      SQLINTEGER FAR * pfNativeError, SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	return _SQLGetDiagRec(handleType, handle, numRecord, szSqlState, pfNativeError, szErrorMsg, cbErrorMsgMax, pcbErrorMsg);
}

SQLRETURN SQL_API
SQLGetDiagField(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLSMALLINT diagIdentifier, SQLPOINTER buffer,
		SQLSMALLINT cbBuffer, SQLSMALLINT FAR * pcbBuffer)
{
	SQLRETURN result = SQL_SUCCESS;
	struct _sql_errors *errs = NULL;
	const char *msg;
	unsigned char odbc_ver = SQL_OV_ODBC2;
	int cplen;
	TDS_STMT *stmt = NULL;
	TDS_DBC *dbc = NULL;
	TDS_ENV *env = NULL;
	TDSSOCKET *tsock;
	char tmp[16];

	if (cbBuffer < 0 || !handle)
		return SQL_ERROR;

	switch (handleType) {
	case SQL_HANDLE_STMT:
		stmt = ((TDS_STMT *) handle);
		dbc = stmt->hdbc;
		env = dbc->henv;
		errs = &stmt->errs;
		break;

	case SQL_HANDLE_DBC:
		dbc = ((TDS_DBC *) handle);
		env = dbc->henv;
		errs = &dbc->errs;
		break;

	case SQL_HANDLE_ENV:
		env = ((TDS_ENV *) handle);
		errs = &env->errs;
		break;

	default:
		return SQL_INVALID_HANDLE;
	}
	odbc_ver = env->attr.attr_odbc_version;

	/* header (numRecord ignored) */
	switch (diagIdentifier) {
	case SQL_DIAG_DYNAMIC_FUNCTION:
		if (handleType != SQL_HANDLE_STMT)
			return SQL_ERROR;

		/* TODO */
		return odbc_set_string(buffer, cbBuffer, pcbBuffer, "", 0);

	case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
		*(SQLINTEGER *) buffer = 0;
		return SQL_SUCCESS;

	case SQL_DIAG_NUMBER:
		*(SQLINTEGER *) buffer = errs->num_errors;
		return SQL_SUCCESS;

	case SQL_DIAG_RETURNCODE:
		/* TODO check if all warnings or not */
		if (errs->num_errors > 0)
			*(SQLRETURN *) buffer = SQL_ERROR;
		else
			*(SQLRETURN *) buffer = SQL_SUCCESS;
		return SQL_SUCCESS;

	case SQL_DIAG_CURSOR_ROW_COUNT:
		if (stmt == NULL)
			return SQL_ERROR;

		/* TODO */
		*(SQLINTEGER *) buffer = 0;
		return SQL_SUCCESS;

	case SQL_DIAG_ROW_COUNT:
		if (stmt == NULL)
			return SQL_ERROR;

		/* FIXME I'm not sure this is correct. */
		if (stmt->hdbc != NULL && stmt->hdbc->tds_socket != NULL) {
			tsock = stmt->hdbc->tds_socket;
			if (tsock->rows_affected == TDS_NO_COUNT) {
				/* FIXME use row_count ?? */
				if (tsock->res_info != NULL)
					*(SQLINTEGER *) buffer = tsock->res_info->row_count;
				else
					*(SQLINTEGER *) buffer = 0;
			} else
				*(SQLINTEGER *) buffer = tsock->rows_affected;
		} else
			return SQL_ERROR;
		return SQL_SUCCESS;
	}

	if (numRecord > errs->num_errors)
		return SQL_NO_DATA_FOUND;

	if (numRecord <= 0)
		return SQL_ERROR;
	--numRecord;

	switch (diagIdentifier) {
	case SQL_DIAG_ROW_NUMBER:
		*(SQLINTEGER *) buffer = SQL_ROW_NUMBER_UNKNOWN;
		break;

	case SQL_DIAG_CLASS_ORIGIN:
	case SQL_DIAG_SUBCLASS_ORIGIN:
		if (odbc_ver == SQL_OV_ODBC2)
			result = odbc_set_string(buffer, cbBuffer, pcbBuffer, "ISO 9075", -1);
		else
			result = odbc_set_string(buffer, cbBuffer, pcbBuffer, "ODBC 3.0", -1);
		break;

	case SQL_DIAG_COLUMN_NUMBER:
		*(SQLINTEGER *) buffer = SQL_COLUMN_NUMBER_UNKNOWN;
		break;

#ifdef SQL_DIAG_SS_MSGSTATE
	case SQL_DIAG_SS_MSGSTATE:
		if (errs->errs[numRecord].msgstate == 0)
			return SQL_ERROR;
		else
			*(SQLINTEGER *) buffer = errs->errs[numRecord].msgstate;
		break;
#endif

#ifdef SQL_DIAG_SS_LINE
	case SQL_DIAG_SS_LINE:
		if (errs->errs[numRecord].linenum == 0)
			return SQL_ERROR;
		else
			*(SQLUSMALLINT *) buffer = errs->errs[numRecord].linenum;
		break;
#endif

	case SQL_DIAG_CONNECTION_NAME:
		if (dbc && dbc->tds_socket && dbc->tds_socket->spid > 0)
			cplen = sprintf(tmp, "%d", dbc->tds_socket->spid);
		else
			cplen = 0;

		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, tmp, cplen);
		break;

	case SQL_DIAG_MESSAGE_TEXT:
		msg = errs->errs[numRecord].msg;
		if (!msg)
			msg = errs->errs[numRecord].err->msg;

		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, -1);
		break;

	case SQL_DIAG_NATIVE:
		*(SQLINTEGER *) buffer = errs->errs[numRecord].msgnum;
		break;

	case SQL_DIAG_SERVER_NAME:
		/* FIXME connect_info, as documented (or should be) is always NULL */
		if (dbc && dbc->tds_socket && dbc->tds_socket->connect_info != NULL) {
			if ((msg = tds_dstr_cstr(&dbc->tds_socket->connect_info->server_name)) != NULL) {
				result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, -1);
			} else {
				if (pcbBuffer)
					*pcbBuffer = 0;
			}
		} else {
			if (pcbBuffer)
				*pcbBuffer = 0;
		}
		break;

	case SQL_DIAG_SQLSTATE:
		msg = errs->errs[numRecord].sqlstate;
		if (*msg != '\0') {
			if (odbc_ver == SQL_OV_ODBC3)
				sqlstate2to3(errs->errs[numRecord].sqlstate);
		} else if (odbc_ver == SQL_OV_ODBC3)
			msg = errs->errs[numRecord].err->state3;
		else
			msg = errs->errs[numRecord].err->state2;

		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, 5);
		break;

	default:
		return SQL_ERROR;
	}
	return result;
}
#endif
