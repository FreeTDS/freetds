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

static char software_version[] = "$Id: error.c,v 1.23 2003-07-29 06:02:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void odbc_errs_pop(struct _sql_errors *errs);
static const char *odbc_get_msg(const char *sqlstate);
static void odbc_get_sqlstate(TDS_UINT native, char *dest_state);
static void odbc_get_v2state(const char *sqlstate, char *dest_state);
SQLRETURN _SQLRowCount(SQLHSTMT hstmt, SQLINTEGER FAR * pcrow);

struct s_SqlMsgMap
{
	const char *msg;
	char sqlstate[6];
};

/* This list contains both v2 and v3 messages */
#define ODBCERR(s3,msg) { msg, s3 }
static const struct s_SqlMsgMap SqlMsgMap[] = {
	ODBCERR("IM007", "No data source or driver specified"),
	ODBCERR("01000", "Warning"),
	ODBCERR("01002", "Disconnect error"),
	ODBCERR("01004", "Data truncated"),
	ODBCERR("01504", "The UPDATE or DELETE statement does not include a WHERE clause"),
	ODBCERR("01508", "Statement disqualified for blocking"),
	ODBCERR("01S00", "Invalid connection string attribute"),
	ODBCERR("01S01", "Error in row"),
	ODBCERR("01S02", "Option value changed"),
	ODBCERR("01S06", "Attempt to fetch before the result set returned the first rowset"),
	ODBCERR("01S07", "Fractional truncation"),
	ODBCERR("07001", "Wrong number of parameters"),
	ODBCERR("07002", "Too many columns"),
	ODBCERR("07005", "The statement did not return a result set"),
	ODBCERR("07006", "Invalid conversion"),
	ODBCERR("07009", "Invalid descriptor index"),
	ODBCERR("08001", "Unable to connect to data source"),
	ODBCERR("08002", "Connection in use"),
	ODBCERR("08003", "Connection is closed"),
	ODBCERR("08004", "The application server rejected establishment of the connection"),
	ODBCERR("08007", "Connection failure during transaction"),
	ODBCERR("08S01", "Communication link failure"),
	ODBCERR("0F001", "The LOB token variable does not currently represent any value"),
	ODBCERR("21S01", "Insert value list does not match column list"),
	ODBCERR("22001", "String data right truncation"),
	ODBCERR("22002", "Invalid output or indicator buffer specified"),
	ODBCERR("22003", "Numeric value out of range"),
	ODBCERR("22005", "Error in assignment"),
	ODBCERR("22007", "Invalid datetime format"),
	ODBCERR("22008", "Datetime field overflow"),
	ODBCERR("22011", "A substring error occurred"),
	ODBCERR("22012", "Division by zero is invalid"),
	ODBCERR("22015", "Interval field overflow"),
	ODBCERR("22018", "Invalid character value for cast specification"),
	ODBCERR("22019", "Invalid escape character"),
	ODBCERR("22025", "Invalid escape sequence"),
	ODBCERR("22026", "String data, length mismatch"),
	ODBCERR("23000", "Integrity constraint violation"),
	ODBCERR("24000", "Invalid cursor state"),
	ODBCERR("24504", "The cursor identified in the UPDATE, DELETE, SET, or GET statement is not positioned on a row"),
	ODBCERR("25501", "Invalid transaction state"),
	ODBCERR("28000", "Invalid authorization specification"),
	ODBCERR("34000", "Invalid cursor name"),
	ODBCERR("37000", "Invalid SQL syntax"),
	ODBCERR("40001", "Serialization failure"),
	ODBCERR("40003", "Statement completion unknown"),
	ODBCERR("42000", "Syntax error or access violation"),
	ODBCERR("42601", "PARMLIST syntax error"),
	ODBCERR("42818", "The operands of an operator or function are not compatible"),
	ODBCERR("42895", "The value of a host variable in the EXECUTE or OPEN statement cannot be used because of its data type"),
	ODBCERR("428A1", "Unable to access a file referenced by a host file variable"),
	ODBCERR("44000", "Integrity constraint violation"),
	ODBCERR("54028", "The maximum number of concurrent LOB handles has been reached"),
	ODBCERR("56084", "LOB data is not supported in DRDA"),
	ODBCERR("58004", "Unexpected system failure"),
	ODBCERR("HY000", "General driver error"),
	ODBCERR("HY001", "Memory allocation failure"),
	ODBCERR("HY002", "Invalid column number"),
	ODBCERR("HY003", "Program type out of range"),
	ODBCERR("HY004", "Invalid data type"),
	ODBCERR("HY007", "Associated statement is not prepared"),
	ODBCERR("HY008", "Operation was cancelled"),
	ODBCERR("HY009", "Invalid argument value"),
	ODBCERR("HY010", "Function sequence error"),
	ODBCERR("HY011", "Operation invalid at this time"),
	ODBCERR("HY012", "Invalid transaction code"),
	ODBCERR("HY013", "Unexpected memory handling error"),
	ODBCERR("HY014", "No more handles"),
	ODBCERR("HY016", "Cannot modify an implementation row descriptor"),
	ODBCERR("HY017", "Invalid use of an automatically allocated descriptor handle"),
	ODBCERR("HY018", "Server declined cancel request"),
	ODBCERR("HY021", "Inconsistent descriptor information"),
	ODBCERR("HY024", "Invalid attribute value"),
	ODBCERR("HY090", "Invalid string or buffer length"),
	ODBCERR("HY091", "Descriptor type out of range"),
	ODBCERR("HY092", "Invalid option"),
	ODBCERR("HY093", "Invalid parameter number"),
	ODBCERR("HY094", "Invalid scale value"),
	ODBCERR("HY096", "Information type out of range"),
	ODBCERR("HY097", "Column type out of range"),
	ODBCERR("HY098", "Scope type out of range"),
	ODBCERR("HY099", "Nullable type out of range"),
	ODBCERR("HY100", "Uniqueness option type out of range"),
	ODBCERR("HY101", "Accuracy option type out of range"),
	ODBCERR("HY103", "Direction option out of range"),
	ODBCERR("HY104", "Invalid precision value"),
	ODBCERR("HY105", "Invalid parameter type"),
	ODBCERR("HY106", "Fetch type out of range"),
	ODBCERR("HY107", "Row value out of range"),
	ODBCERR("HY109", "Invalid cursor position"),
	ODBCERR("HY110", "Invalid driver completion"),
	ODBCERR("HY111", "Invalid bookmark value"),
	ODBCERR("HY501", "Invalid data source name"),
	ODBCERR("HY503", "Invalid file name length"),
	ODBCERR("HY506", "Error closing a file"),
	ODBCERR("HY509", "Error deleting a file"),
	ODBCERR("HYC00", "Driver not capable"),
	ODBCERR("HYT00", "Timeout expired"),
	ODBCERR("HYT01", "Connection timeout expired"),
	ODBCERR("S0001", "Database object already exists"),
	ODBCERR("S0002", "Database object does not exist"),
	ODBCERR("S0011", "Index already exists"),
	ODBCERR("S0012", "Index not found"),
	ODBCERR("S0021", "Column already exists"),
	ODBCERR("S0022", "Column not found"),
	ODBCERR("", NULL)
};

#undef ODBCERR

struct s_NativeToSqlstateMap
{
	TDS_UINT native;
	char sqlstate[6];
};

/* TODO use function in token.c */
/* Try to map a native error code to a SQLSTATE */
static const struct s_NativeToSqlstateMap NativeToSqlstateMap[] = {
	{0, "42000"},
	{109, "21S01"},
	{110, "21S01"},
	{207, "42S22"},
	{208, "42S02"},
	{245, "22018"},
	{295, "22007"},
	{296, "22007"},
	{3621, "01000"},
	{2627, "23000"},
	{18456, "28000"},
	{8152, "22001"},
	{0, ""}
};

struct s_v3to2map
{
	char v3[6];
	char v2[6];
};

/* Map a v3 SQLSTATE to a v2 */
static const struct s_v3to2map v3to2map[] = {
	{"01001", "01S03"},
	{"01001", "01S04"},
	{"HY019", "22003"},
	{"22007", "22008"},
	{"22018", "22005"},
	{"07005", "24000"},
	{"42000", "37000"},
	{"HY018", "70100"},
	{"42S01", "S0001"},
	{"42S02", "S0002"},
	{"42S11", "S0011"},
	{"42S12", "S0012"},
	{"42S21", "S0021"},
	{"42S22", "S0022"},
	{"42S23", "S0023"},
	{"HY000", "S1000"},
	{"HY001", "S1001"},
	{"07009", "S1002"},
	{"HY003", "S1003"},
	{"HY004", "S1004"},
	{"HY008", "S1008"},
	{"HY009", "S1009"},
	{"HY024", "S1009"},
	{"HY007", "S1010"},
	{"HY010", "S1010"},
	{"HY011", "S1011"},
	{"HY012", "S1012"},
	{"HY090", "S1090"},
	{"HY091", "S1091"},
	{"HY092", "S1092"},
/*	{"07009", "S1093"}, */
	{"HY096", "S1096"},
	{"HY097", "S1097"},
	{"HY098", "S1098"},
	{"HY099", "S1099"},
	{"HY100", "S1100"},
	{"HY101", "S1101"},
	{"HY103", "S1103"},
	{"HY104", "S1104"},
	{"HY105", "S1105"},
	{"HY106", "S1106"},
	{"HY107", "S1107"},
	{"HY108", "S1108"},
	{"HY109", "S1109"},
	{"HY110", "S1110"},
	{"HY111", "S1111"},
	{"HYC00", "S1C00"},
	{"HYT00", "S1T00"},
	{"08001", "S1000"},
	{"IM007", "S1000"},
	{"", ""}
};

static const char *
odbc_get_msg(const char *sqlstate)
{
	const struct s_SqlMsgMap *pmap = SqlMsgMap;

	/* TODO set flag and use pointers (no strdup) ?? */
	while (pmap->msg) {
		if (!strcasecmp(sqlstate, pmap->sqlstate)) {
			return strdup(pmap->msg);
		}
		++pmap;
	}
	return strdup("");
}

/* TODO use function in token.c */
static void
odbc_get_sqlstate(TDS_UINT native, char *dest_state)
{
	const struct s_NativeToSqlstateMap *pmap = NativeToSqlstateMap;

	while (pmap->sqlstate[0]) {
		if (pmap->native == native) {
			strncpy(dest_state, pmap->sqlstate, 5);
			return;
		}
		++pmap;
	}
	/* The default sqlstate */
	strncpy(dest_state, "42000", 5);
}

static void
odbc_get_v2state(const char *sqlstate, char *dest_state)
{
	const struct s_v3to2map *pmap = v3to2map;

	while (pmap->v3[0]) {
		if (!strcasecmp(pmap->v3, sqlstate)) {
			strncpy(dest_state, pmap->v2, 5);
			return;
		}
		++pmap;
	}
	/* return the original if a v2 state is not found */
	strncpy(dest_state, sqlstate, 5);
}

void
odbc_errs_reset(struct _sql_errors *errs)
{
	int i;

	if (errs->errs) {
		for (i = 0; i < errs->num_errors; ++i) {
			/* TODO see flags */
			if (errs->errs[i].msg)
				free((char *) errs->errs[i].msg);
			if (errs->errs[i].server)
				free(errs->errs[i].server);
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
		free((char *) errs->errs[0].msg);
	if (errs->errs[0].server)
		free(errs->errs[0].server);

	--errs->num_errors;
	memmove(&(errs->errs[0]), &(errs->errs[1]), errs->num_errors * sizeof(errs->errs[0]));
}

void
odbc_errs_add(struct _sql_errors *errs, TDS_UINT native, const char *sqlstate, const char *msg, const char *server)
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

	memset(p, 0, sizeof(*p));
	errs->errs[n].native = native;
	if (sqlstate) {
		strncpy(errs->errs[n].state3, sqlstate, 5);
		errs->errs[n].state3[5] = '\0';
	} else
		odbc_get_sqlstate(native, errs->errs[n].state3);
	odbc_get_v2state(errs->errs[n].state3, errs->errs[n].state2);
	/* TODO why driver ?? -- freddy77 */
	errs->errs[n].server = (server) ? strdup(server) : strdup("DRIVER");
	errs->errs[n].msg = msg ? strdup(msg) : odbc_get_msg(errs->errs[n].state3);
	++errs->num_errors;
}

/* TODO check if TDS_UINT is correct for native error */
void
odbc_errs_add_rdbms(struct _sql_errors *errs, TDS_UINT native, const char *sqlstate, const char *msg, int linenum, int msgstate,
		    const char *server)
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

	memset(p, 0, sizeof(*p));
	errs->errs[n].native = native;
	if (sqlstate) {
		/* FIXME this can cause all error to be wrong !!! */
		/* TODO is correct to set state3 ?? */
		strncpy(errs->errs[n].state3, sqlstate, 5);
		errs->errs[n].state3[5] = '\0';
	} else
		odbc_get_sqlstate(native, errs->errs[n].state3);
	odbc_get_v2state(errs->errs[n].state3, errs->errs[n].state2);
	/* TODO why driver ?? -- freddy77 */
	errs->errs[n].server = (server) ? strdup(server) : strdup("DRIVER");
	errs->errs[n].msg = msg ? strdup(msg) : odbc_get_msg(errs->errs[n].state3);
	errs->errs[n].linenum = linenum;
	errs->errs[n].msgstate = msgstate;
	++errs->num_errors;
}

#if 0
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
#endif

static SQLRETURN
_SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle, SQLSMALLINT numRecord, SQLCHAR FAR * szSqlState,
	       SQLINTEGER FAR * pfNativeError, SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR * pcbErrorMsg)
{
	SQLRETURN result;
	struct _sql_errors *errs = NULL;
	const char *msg;
	char *p;

	/* TODO use FreeTDS name ?? */
	static const char msgprefix[] = "[ODBC SQL Server Driver][SQL Server]";

	/* FIXME change type, possible overflow */
	unsigned char odbc_ver = SQL_OV_ODBC2;

	if (numRecord <= 0 || cbErrorMsgMax < 0 || !handle)
		return SQL_ERROR;

	switch (handleType) {
	case SQL_HANDLE_STMT:
		odbc_ver = ((TDS_STMT *) handle)->hdbc->henv->attr.attr_odbc_version;
		errs = &((TDS_STMT *) handle)->errs;
		break;

	case SQL_HANDLE_DBC:
		odbc_ver = ((TDS_DBC *) handle)->henv->attr.attr_odbc_version;
		errs = &((TDS_DBC *) handle)->errs;
		break;

	case SQL_HANDLE_ENV:
		odbc_ver = ((TDS_ENV *) handle)->attr.attr_odbc_version;
		errs = &((TDS_ENV *) handle)->errs;
		break;

	default:
		return SQL_INVALID_HANDLE;
	}

	if (numRecord > errs->num_errors)
		return SQL_NO_DATA_FOUND;
	--numRecord;

	if (szSqlState) {
		if (odbc_ver == SQL_OV_ODBC3)
			strcpy((char *) szSqlState, errs->errs[numRecord].state3);
		else
			strcpy((char *) szSqlState, errs->errs[numRecord].state2);
	}

	msg = errs->errs[numRecord].msg;

	if (asprintf(&p, "%s%s", msgprefix, msg) < 0)
		return SQL_ERROR;
	result = odbc_set_string(szErrorMsg, cbErrorMsgMax, pcbErrorMsg, p, -1);
	free(p);

	if (pfNativeError)
		*pfNativeError = errs->errs[numRecord].native;

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

	/* FIXME possible int overflow, change type */
	unsigned char odbc_ver = SQL_OV_ODBC2;
	int cplen;
	TDS_STMT *stmt = NULL;
	TDS_DBC *dbc = NULL;
	TDS_ENV *env = NULL;
	char tmp[16];
	SQLRETURN lastrc;

	if (cbBuffer < 0)
		return SQL_ERROR;

	if (!handle)
		return SQL_INVALID_HANDLE;

	switch (handleType) {
	case SQL_HANDLE_STMT:
		stmt = ((TDS_STMT *) handle);
		dbc = stmt->hdbc;
		env = dbc->henv;
		errs = &stmt->errs;
		lastrc = stmt->lastrc;
		break;

	case SQL_HANDLE_DBC:
		dbc = ((TDS_DBC *) handle);
		env = dbc->henv;
		errs = &dbc->errs;
		lastrc = dbc->lastrc;
		break;

	case SQL_HANDLE_ENV:
		env = ((TDS_ENV *) handle);
		errs = &env->errs;
		lastrc = env->lastrc;
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
		*(SQLRETURN *) buffer = lastrc;
		return SQL_SUCCESS;

	case SQL_DIAG_CURSOR_ROW_COUNT:
		if (handleType != SQL_HANDLE_STMT)
			return SQL_ERROR;

		/* TODO */
		*(SQLINTEGER *) buffer = 0;
		return SQL_SUCCESS;

	case SQL_DIAG_ROW_COUNT:
		if (handleType != SQL_HANDLE_STMT)
			return SQL_ERROR;

		return _SQLRowCount((SQLHSTMT) handle, (SQLINTEGER FAR *) buffer);
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
		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, -1);
		break;

	case SQL_DIAG_NATIVE:
		*(SQLINTEGER *) buffer = errs->errs[numRecord].native;
		break;

	case SQL_DIAG_SERVER_NAME:
		msg = "";
		switch (handleType) {
		case SQL_HANDLE_ENV:
			break;
		case SQL_HANDLE_DBC:
			msg = tds_dstr_cstr(&((TDS_DBC *) handle)->server);
			break;
		case SQL_HANDLE_STMT:
			msg = tds_dstr_cstr(&((TDS_STMT *) handle)->hdbc->server);
			/* if hdbc->server is not initialized, init it
			 * from the errs structure */
			if (!msg[0] && errs->errs[numRecord].server) {
				tds_dstr_copy(&((TDS_STMT *) handle)->hdbc->server, errs->errs[numRecord].server);
				msg = errs->errs[numRecord].server;
			}
			break;
		}
		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, -1);
		break;

	case SQL_DIAG_SQLSTATE:
		if (odbc_ver == SQL_OV_ODBC3)
			msg = errs->errs[numRecord].state3;
		else
			msg = errs->errs[numRecord].state2;

		result = odbc_set_string(buffer, cbBuffer, pcbBuffer, msg, 5);
		break;

	default:
		return SQL_ERROR;
	}
	return result;
}
#endif
