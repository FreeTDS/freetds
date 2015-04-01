/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008  Frediano Ziglio
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

/* $Id: odbcss.h,v 1.1 2008-05-07 08:42:30 freddy77 Exp $ */
#ifndef _odbcss_h_
#define _odbcss_h_

#ifdef TDSODBC_BCP
#include <sql.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SQL_DIAG_SS_MSGSTATE	(-1150)
#define SQL_DIAG_SS_LINE	(-1154)

#define SQL_SOPT_SS_QUERYNOTIFICATION_TIMEOUT  1233
#define SQL_SOPT_SS_QUERYNOTIFICATION_MSGTEXT  1234
#define SQL_SOPT_SS_QUERYNOTIFICATION_OPTIONS  1235

#ifndef SQL_SS_LENGTH_UNLIMITED
#define SQL_SS_LENGTH_UNLIMITED 0
#endif

#ifndef SQL_COPT_SS_OLDPWD
#define SQL_COPT_SS_OLDPWD 1226
#endif

#define SQL_COPT_SS_BASE	1200

#ifdef TDSODBC_BCP

#ifndef SUCCEED
#define SUCCEED 1
#endif
#ifndef FAIL
#define FAIL 0
#endif

#ifndef BCPKEEPIDENTITY
#define BCPKEEPIDENTITY 8
#endif
#ifndef BCPHINTS
#define BCPHINTS 6
#endif

#define BCP_DIRECTION_IN 1

#define SQL_COPT_SS_BCP	(SQL_COPT_SS_BASE+19)
#define SQL_BCP_OFF 0
#define SQL_BCP_ON 1

#define SQL_COPT_TDSODBC_IMPL_BASE	1500
#define SQL_COPT_TDSODBC_IMPL_BCP_INITA	(SQL_COPT_TDSODBC_IMPL_BASE)
#define SQL_COPT_TDSODBC_IMPL_BCP_CONTROL	(SQL_COPT_TDSODBC_IMPL_BASE+1)
#define SQL_COPT_TDSODBC_IMPL_BCP_COLPTR	(SQL_COPT_TDSODBC_IMPL_BASE+2)
#define SQL_COPT_TDSODBC_IMPL_BCP_SENDROW	(SQL_COPT_TDSODBC_IMPL_BASE+3)
#define SQL_COPT_TDSODBC_IMPL_BCP_BATCH	(SQL_COPT_TDSODBC_IMPL_BASE+4)
#define SQL_COPT_TDSODBC_IMPL_BCP_DONE	(SQL_COPT_TDSODBC_IMPL_BASE+5)
#define SQL_COPT_TDSODBC_IMPL_BCP_BIND	(SQL_COPT_TDSODBC_IMPL_BASE+6)

#define SQL_VARLEN_DATA -10

/* copied from sybdb.h which was copied from tds.h */
/* TODO find a much better way... */
enum
{
	BCP_TYPE_SQLCHAR = 47,		/* 0x2F */
#define BCP_TYPE_SQLCHAR	BCP_TYPE_SQLCHAR
	BCP_TYPE_SQLVARCHAR = 39,	/* 0x27 */
#define BCP_TYPE_SQLVARCHAR	BCP_TYPE_SQLVARCHAR
	BCP_TYPE_SQLINTN = 38,		/* 0x26 */
#define BCP_TYPE_SQLINTN	BCP_TYPE_SQLINTN
	BCP_TYPE_SQLINT1 = 48,		/* 0x30 */
#define BCP_TYPE_SQLINT1	BCP_TYPE_SQLINT1
	BCP_TYPE_SQLINT2 = 52,		/* 0x34 */
#define BCP_TYPE_SQLINT2	BCP_TYPE_SQLINT2
	BCP_TYPE_SQLINT4 = 56,		/* 0x38 */
#define BCP_TYPE_SQLINT4	BCP_TYPE_SQLINT4
	BCP_TYPE_SQLINT8 = 127,		/* 0x7F */
#define BCP_TYPE_SQLINT8	BCP_TYPE_SQLINT8
	BCP_TYPE_SQLFLT8 = 62,		/* 0x3E */
#define BCP_TYPE_SQLFLT8	BCP_TYPE_SQLFLT8
	BCP_TYPE_SQLDATETIME = 61,	/* 0x3D */
#define BCP_TYPE_SQLDATETIME	BCP_TYPE_SQLDATETIME
	BCP_TYPE_SQLBIT = 50,		/* 0x32 */
#define BCP_TYPE_SQLBIT	BCP_TYPE_SQLBIT
	BCP_TYPE_SQLBITN = 104,		/* 0x68 */
#define BCP_TYPE_SQLBITN	BCP_TYPE_SQLBITN
	BCP_TYPE_SQLTEXT = 35,		/* 0x23 */
#define BCP_TYPE_SQLTEXT	BCP_TYPE_SQLTEXT
	BCP_TYPE_SQLNTEXT = 99,		/* 0x63 */
#define BCP_TYPE_SQLNTEXT	BCP_TYPE_SQLNTEXT
	BCP_TYPE_SQLIMAGE = 34,		/* 0x22 */
#define BCP_TYPE_SQLIMAGE	BCP_TYPE_SQLIMAGE
	BCP_TYPE_SQLMONEY4 = 122,	/* 0x7A */
#define BCP_TYPE_SQLMONEY4	BCP_TYPE_SQLMONEY4
	BCP_TYPE_SQLMONEY = 60,		/* 0x3C */
#define BCP_TYPE_SQLMONEY	BCP_TYPE_SQLMONEY
	BCP_TYPE_SQLDATETIME4 = 58,	/* 0x3A */
#define BCP_TYPE_SQLDATETIME4	BCP_TYPE_SQLDATETIME4
	BCP_TYPE_SQLREAL = 59,		/* 0x3B */
	BCP_TYPE_SQLFLT4 = 59,		/* 0x3B */
#define BCP_TYPE_SQLREAL	BCP_TYPE_SQLREAL
#define BCP_TYPE_SQLFLT4	BCP_TYPE_SQLFLT4
	BCP_TYPE_SQLBINARY = 45,		/* 0x2D */
#define BCP_TYPE_SQLBINARY	BCP_TYPE_SQLBINARY
	BCP_TYPE_SQLVOID = 31,		/* 0x1F */
#define BCP_TYPE_SQLVOID	BCP_TYPE_SQLVOID
	BCP_TYPE_SQLVARBINARY = 37,	/* 0x25 */
#define BCP_TYPE_SQLVARBINARY	BCP_TYPE_SQLVARBINARY
	BCP_TYPE_SQLNUMERIC = 108,	/* 0x6C */
#define BCP_TYPE_SQLNUMERIC	BCP_TYPE_SQLNUMERIC
	BCP_TYPE_SQLDECIMAL = 106,	/* 0x6A */
#define BCP_TYPE_SQLDECIMAL	BCP_TYPE_SQLDECIMAL
	BCP_TYPE_SQLFLTN = 109,		/* 0x6D */
#define BCP_TYPE_SQLFLTN	BCP_TYPE_SQLFLTN
	BCP_TYPE_SQLMONEYN = 110,	/* 0x6E */
#define BCP_TYPE_SQLMONEYN	BCP_TYPE_SQLMONEYN
	BCP_TYPE_SQLDATETIMN = 111,	/* 0x6F */
#define BCP_TYPE_SQLDATETIMN	BCP_TYPE_SQLDATETIMN
	BCP_TYPE_SQLNVARCHAR = 103,	/* 0x67 */
#define BCP_TYPE_SQLNVARCHAR	BCP_TYPE_SQLNVARCHAR
	BCP_TYPE_SQLUNIQUEID = 36,	/* 0x24 */
#define BCP_TYPE_SQLUNIQUEID	BCP_TYPE_SQLUNIQUEID
	BCP_TYPE_SQLDATETIME2 = 42,    /* 0x2a */
#define BCP_TYPE_SQLDATETIME2	BCP_TYPE_SQLDATETIME2
};

typedef struct
{
	int dtdays;
	int dttime;
} DBDATETIME;

#ifdef _MSC_VER
#define TDSODBC_INLINE __inline
#else
#define TDSODBC_INLINE __inline__
#endif

struct tdsodbc_impl_bcp_init_params
{
	const char *tblname;
	const char *hfile;
	const char *errfile;
	int direction;
};

static TDSODBC_INLINE RETCODE
bcp_initA(HDBC hdbc, const char *tblname, const char *hfile, const char *errfile, int direction)
{
	struct tdsodbc_impl_bcp_init_params params = {tblname, hfile, errfile, direction};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_INITA, &params, SQL_IS_POINTER)) ? SUCCEED : FAIL;
}

struct tdsodbc_impl_bcp_control_params
{
	int field;
	void *value;
};

static TDSODBC_INLINE RETCODE
bcp_control(HDBC hdbc, int field, void *value)
{
	struct tdsodbc_impl_bcp_control_params params = {field, value};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_CONTROL, &params, SQL_IS_POINTER)) ? SUCCEED : FAIL;
}

struct tdsodbc_impl_bcp_colptr_params
{
	const BYTE * colptr;
	int table_column;
};

static TDSODBC_INLINE RETCODE
bcp_colptr(HDBC hdbc, const BYTE * colptr, int table_column)
{
	struct tdsodbc_impl_bcp_colptr_params params = {colptr, table_column};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_COLPTR, &params, SQL_IS_POINTER)) ? SUCCEED : FAIL;
}

static TDSODBC_INLINE RETCODE
bcp_sendrow(HDBC hdbc)
{
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_SENDROW, NULL, SQL_IS_POINTER)) ? SUCCEED : FAIL;
}

struct tdsodbc_impl_bcp_batch_params
{
	int rows;
};

static TDSODBC_INLINE int
bcp_batch(HDBC hdbc)
{
	struct tdsodbc_impl_bcp_batch_params params = {-1};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_BATCH, &params, SQL_IS_POINTER)) ? params.rows : -1;
}

struct tdsodbc_impl_bcp_done_params
{
	int rows;
};

static TDSODBC_INLINE int
bcp_done(HDBC hdbc)
{
	struct tdsodbc_impl_bcp_done_params params = {-1};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_DONE, &params, SQL_IS_POINTER)) ? params.rows : -1;
}

struct tdsodbc_impl_bcp_bind_params
{
	const BYTE * varaddr;
	int prefixlen;
	int varlen;
	const BYTE * terminator;
	int termlen;
	int vartype;
	int table_column;
};

static TDSODBC_INLINE RETCODE
bcp_bind(HDBC hdbc, const BYTE * varaddr, int prefixlen, int varlen,
	const BYTE * terminator, int termlen, int vartype, int table_column)
{
	struct tdsodbc_impl_bcp_bind_params params = {varaddr, prefixlen, varlen, terminator, termlen, vartype, table_column};
	return SQL_SUCCEEDED(SQLSetConnectAttr(hdbc, SQL_COPT_TDSODBC_IMPL_BCP_BIND, &params, SQL_IS_POINTER)) ? SUCCEED : FAIL;
}

#define bcp_init bcp_initA

#endif /* TDSODBC_BCP */

#ifdef __cplusplus
}
#endif

#endif /* _odbcss_h_ */
