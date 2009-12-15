#ifdef WIN32
#include <windows.h>
#include <direct.h>
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sql.h>
#include <sqlext.h>

static char rcsid_common_h[] = "$Id: common.h,v 1.29 2009-12-15 11:23:47 freddy77 Exp $";
static void *no_unused_common_h_warn[] = { rcsid_common_h, no_unused_common_h_warn };

#ifndef HAVE_SQLLEN
#ifndef SQLULEN
#define SQLULEN SQLUINTEGER
#endif
#ifndef SQLLEN
#define SQLLEN SQLINTEGER
#endif
#endif

extern HENV Environment;
extern HDBC Connection;
extern HSTMT Statement;
extern int use_odbc_version3;
extern void (*odbc_set_conn_attr)(void);

extern char USER[512];
extern char SERVER[512];
extern char PASSWORD[512];
extern char DATABASE[512];
extern char DRIVER[1024];

int read_login_info(void);
void ReportError(const char *msg, int line, const char *file);

void CheckCols(int n, int line, const char * file);
void CheckRows(int n, int line, const char * file);
#define CHECK_ROWS(n) CheckRows(n, __LINE__, __FILE__)
#define CHECK_COLS(n) CheckCols(n, __LINE__, __FILE__)
void ResetStatementProc(SQLHSTMT *stmt, const char *file, int line);
#define ResetStatement() ResetStatementProc(&Statement, __FILE__, __LINE__)
void CheckCursor(void);

#define ODBC_REPORT_ERROR(msg) ReportError(msg, __LINE__, __FILE__)

SQLRETURN CheckRes(const char *file, int line, SQLRETURN rc, SQLSMALLINT handle_type, SQLHANDLE handle, const char *func, const char *res);
#define CHKR(func, params, res) \
	CheckRes(__FILE__, __LINE__, (func params), 0, 0, #func, res)
#define CHKR2(func, params, type, handle, res) \
	CheckRes(__FILE__, __LINE__, (func params), type, handle, #func, res)

SQLSMALLINT AllocHandleErrType(SQLSMALLINT type);

#define CHKAllocConnect(a,res) \
	CHKR2(SQLAllocConnect, (Environment,a), SQL_HANDLE_ENV, Environment, res)
#define CHKAllocEnv(a,res) \
	CHKR2(SQLAllocEnv, (a), 0, 0, res)
#define CHKAllocStmt(a,res) \
	CHKR2(SQLAllocStmt, (Connection,a), SQL_HANDLE_DBC, Connection, res)
#define CHKAllocHandle(a,b,c,res) \
	CHKR2(SQLAllocHandle, (a,b,c), AllocHandleErrType(a), b, res)
#define CHKBindCol(a,b,c,d,e,res) \
	CHKR2(SQLBindCol, (Statement,a,b,c,d,e), SQL_HANDLE_STMT, Statement, res)
#define CHKBindParameter(a,b,c,d,e,f,g,h,i,res) \
	CHKR2(SQLBindParameter, (Statement,a,b,c,d,e,f,g,h,i), SQL_HANDLE_STMT, Statement, res)
#define CHKCancel(res) \
	CHKR2(SQLCancel, (Statement), SQL_HANDLE_STMT, Statement, res)
#define CHKCloseCursor(res) \
	CHKR2(SQLCloseCursor, (Statement), SQL_HANDLE_STMT, Statement, res)
#define CHKColAttribute(a,b,c,d,e,f,res) \
	CHKR2(SQLColAttribute, (Statement,a,b,c,d,e,f), SQL_HANDLE_STMT, Statement, res)
#define CHKDescribeCol(a,b,c,d,e,f,g,h,res) \
	CHKR2(SQLDescribeCol, (Statement,a,b,c,d,e,f,g,h), SQL_HANDLE_STMT, Statement, res)
#define CHKDriverConnect(a,b,c,d,e,f,g,res) \
	CHKR2(SQLDriverConnect, (Connection,a,b,c,d,e,f,g), SQL_HANDLE_DBC, Connection, res)
#define CHKEndTran(a,b,c,res) \
	CHKR2(SQLEndTran, (a,b,c), a, b, res)
#define CHKExecDirect(a,b,res) \
	CHKR2(SQLExecDirect, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKExecute(res) \
	CHKR2(SQLExecute, (Statement), SQL_HANDLE_STMT, Statement, res)
#define CHKExtendedFetch(a,b,c,d,res) \
	CHKR2(SQLExtendedFetch, (Statement,a,b,c,d), SQL_HANDLE_STMT, Statement, res)
#define CHKFetch(res) \
	CHKR2(SQLFetch, (Statement), SQL_HANDLE_STMT, Statement, res)
#define CHKFetchScroll(a,b,res) \
	CHKR2(SQLFetchScroll, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKFreeHandle(a,b,res) \
	CHKR2(SQLFreeHandle, (a,b), a, b, res)
#define CHKFreeStmt(a,res) \
	CHKR2(SQLFreeStmt, (Statement,a), SQL_HANDLE_STMT, Statement, res)
#define CHKGetConnectAttr(a,b,c,d,res) \
	CHKR2(SQLGetConnectAttr, (Connection,a,b,c,d), SQL_HANDLE_DBC, Connection, res)
#define CHKGetDiagRec(a,b,c,d,e,f,g,h,res) \
	CHKR2(SQLGetDiagRec, (a,b,c,d,e,f,g,h), a, b, res)
#define CHKGetStmtAttr(a,b,c,d,res) \
	CHKR2(SQLGetStmtAttr, (Statement,a,b,c,d), SQL_HANDLE_STMT, Statement, res)
#define CHKGetTypeInfo(a,res) \
	CHKR2(SQLGetTypeInfo, (Statement,a), SQL_HANDLE_STMT, Statement, res)
#define CHKGetData(a,b,c,d,e,res) \
	CHKR2(SQLGetData, (Statement,a,b,c,d,e), SQL_HANDLE_STMT, Statement, res)
#define CHKMoreResults(res) \
	CHKR2(SQLMoreResults, (Statement), SQL_HANDLE_STMT, Statement, res)
#define CHKNumResultCols(a,res) \
	CHKR2(SQLNumResultCols, (Statement,a), SQL_HANDLE_STMT, Statement, res)
#define CHKParamData(a,res) \
	CHKR2(SQLParamData, (Statement,a), SQL_HANDLE_STMT, Statement, res)
#define CHKParamOptions(a,b,res) \
	CHKR2(SQLParamOptions, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKPrepare(a,b,res) \
	CHKR2(SQLPrepare, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKPutData(a,b,res) \
	CHKR2(SQLPutData, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKRowCount(a,res) \
	CHKR2(SQLRowCount, (Statement,a), SQL_HANDLE_STMT, Statement, res)
#define CHKSetConnectAttr(a,b,c,res) \
	CHKR2(SQLSetConnectAttr, (Connection,a,b,c), SQL_HANDLE_DBC, Connection, res)
#define CHKSetCursorName(a,b,res) \
	CHKR2(SQLSetCursorName, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKSetPos(a,b,c,res) \
	CHKR2(SQLSetPos, (Statement,a,b,c), SQL_HANDLE_STMT, Statement, res)
#define CHKSetStmtAttr(a,b,c,res) \
	CHKR2(SQLSetStmtAttr, (Statement,a,b,c), SQL_HANDLE_STMT, Statement, res)
#define CHKSetStmtOption(a,b,res) \
	CHKR2(SQLSetStmtOption, (Statement,a,b), SQL_HANDLE_STMT, Statement, res)
#define CHKTables(a,b,c,d,e,f,g,h,res) \
	CHKR2(SQLTables, (Statement,a,b,c,d,e,f,g,h), SQL_HANDLE_STMT, Statement, res)
#define CHKProcedureColumns(a,b,c,d,e,f,g,h,res) \
	CHKR2(SQLProcedureColumns, (Statement,a,b,c,d,e,f,g,h), SQL_HANDLE_STMT, Statement, res)
#define CHKColumns(a,b,c,d,e,f,g,h,res) \
	CHKR2(SQLColumns, (Statement,a,b,c,d,e,f,g,h), SQL_HANDLE_STMT, Statement, res)

int Connect(void);
int Disconnect(void);
SQLRETURN CommandProc(HSTMT stmt, const char *command, const char *file, int line, const char *res);
#define Command(cmd) CommandProc(Statement, cmd, __FILE__, __LINE__, "SNo")
#define Command2(cmd, res) CommandProc(Statement, cmd, __FILE__, __LINE__, res)
SQLRETURN CommandWithResult(HSTMT stmt, const char *command);
int db_is_microsoft(void);
const char *db_version(void);
unsigned int db_version_int(void);
int driver_is_freetds(void);

#define int2ptr(i) ((void*)(((char*)0)+(i)))
#define ptr2int(p) ((int)(((char*)(p))-((char*)0)))

#if !HAVE_SETENV
void odbc_setenv(const char *name, const char *value, int overwrite);

#define setenv odbc_setenv
#endif
