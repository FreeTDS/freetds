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
#include "tdsodbc.h"
#include "odbc_util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: error.c,v 1.3 2003-01-05 10:28:32 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

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

	for (i = 0; i < errs->num_errors; ++i) {
		if (errs->errs[i].msg)
			free(errs->errs[i].msg);
	}
	free(errs->errs);

	errs->errs = NULL;
	errs->num_errors = 0;
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
	++errs->num_errors;
}

static SQLRETURN
_SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLCHAR FAR * szSqlState,
	       SQLINTEGER FAR * pfNativeError, SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	SQLRETURN result = SQL_SUCCESS;
	struct _sql_errors *errs = NULL;
	const char *msg;
	unsigned char odbc_ver = 2;
	int cplen;

	if (numRecord <= 0 || cbErrorMsgMax < 0 || !handle)
		return SQL_ERROR;

	switch (handleType) {
	case SQL_HANDLE_STMT:
		odbc_ver = ((TDS_STMT *) handle)->hdbc->henv->odbc_ver;
		errs = &((TDS_STMT *) handle)->errs;
		break;

	case SQL_HANDLE_DBC:
		odbc_ver = ((TDS_DBC *) handle)->henv->odbc_ver;
		errs = &((TDS_DBC *) handle)->errs;
		break;

	case SQL_HANDLE_ENV:
		odbc_ver = ((TDS_ENV *) handle)->odbc_ver;
		errs = &((TDS_ENV *) handle)->errs;
		break;

	default:
		return SQL_INVALID_HANDLE;
	}

	if (numRecord > errs->num_errors)
		return SQL_NO_DATA_FOUND;

	if (szSqlState) {
		if (odbc_ver == 3)
			strcpy((char *) szSqlState, errs->errs[0].err->state3);
		else
			strcpy((char *) szSqlState, errs->errs[0].err->state2);
	}

	msg = errs->errs[0].msg;
	if (!msg)
		msg = errs->errs[0].err->msg;
	cplen = strlen(msg);
	if (pcbErrorMsg)
		*pcbErrorMsg = cplen;
	if (cplen >= cbErrorMsgMax) {
		cplen = cbErrorMsgMax - 1;
		result = SQL_SUCCESS_WITH_INFO;
	}
	if (szErrorMsg && cplen >= 0) {
		strncpy((char *) szErrorMsg, msg, cplen);
		((char *) szErrorMsg)[cplen] = 0;
	}
	/* TODO what to return ?? */
	if (pfNativeError)
		*pfNativeError = 1;

	return result;
}

SQLRETURN SQL_API
SQLError(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt, SQLCHAR FAR * szSqlState, SQLINTEGER FAR * pfNativeError,
	 SQLCHAR FAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	SQLRETURN result = SQL_INVALID_HANDLE;
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
	}

	if (errs) {
		result = _SQLGetDiagRec(type, handle, 1, szSqlState, pfNativeError, szErrorMsg, cbErrorMsgMax, pcbErrorMsg);

		if (result == SQL_SUCCESS)
			odbc_errs_reset(errs);
	}

	return result;
}

SQLRETURN SQL_API
SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLCHAR FAR * szSqlState,
	      SQLINTEGER FAR * pfNativeError, SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	return _SQLGetDiagRec(handleType, handle, numRecord, szSqlState, pfNativeError, szErrorMsg, cbErrorMsgMax, pcbErrorMsg);
}
