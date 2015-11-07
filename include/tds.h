/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2010, 2011  Frediano Ziglio
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

#ifndef _tds_h_
#define _tds_h_

/* $Id: tds.h,v 1.352.2.4 2011-08-12 16:29:36 freddy77 Exp $ */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NET_INET_IN_H */
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

/* forward declaration */
typedef struct tdsiconvinfo TDSICONV;
typedef struct tds_socket TDSSOCKET;

#include "tdsver.h"
#include "tds_sysdep_public.h"
#ifdef _FREETDS_LIBRARY_SOURCE
#include "tds_sysdep_private.h"
#endif /* _FREETDS_LIBRARY_SOURCE */

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility push(hidden)
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

/**
 * A structure to hold all the compile-time settings.
 * This structure is returned by tds_get_compiletime_settings
 */

typedef struct tds_compiletime_settings
{
	const char *freetds_version;	/* release version of FreeTDS */
	const char *sysconfdir;		/* location of freetds.conf */
	const char *last_update;	/* latest software_version date among the modules */
	int msdblib;		/* for MS style dblib */
	int sybase_compat;	/* enable increased Open Client binary compatibility */
	int threadsafe;		/* compile for thread safety default=no */
	int libiconv;		/* search for libiconv in DIR/include and DIR/lib */
	const char *tdsver;	/* TDS protocol version (4.2/4.6/5.0/7.0/8.0) 5.0 */
	int iodbc;		/* build odbc driver against iODBC in DIR */
	int unixodbc;		/* build odbc driver against unixODBC in DIR */

} TDS_COMPILETIME_SETTINGS;

typedef struct tds_dstr {
	char *dstr_s;
	size_t dstr_size;
} DSTR;

/**
 * @file tds.h
 * Main include file for libtds
 */

/**
 * \defgroup libtds LibTDS API
 * Callable functions in \c libtds.
 * 
 * The \c libtds library is for use internal to \em FreeTDS.  It is not
 * intended for use by applications.  Although any use is \em permitted, you're
 * encouraged to use one of the established public APIs instead, because their
 * interfaces are stable and documented by the vendors.  
 */

/* 
 * All references to data that touch the wire should use the following typedefs.  
 *
 * If you have problems on 64-bit machines and the code is 
 * using a native datatype, please change it to use
 * these. (In the TDS layer only, the API layers have their
 * own typedefs which equate to these).
 */
typedef char TDS_CHAR;					/*  8-bit char     */
typedef unsigned char TDS_UCHAR;			/*  8-bit uchar    */
typedef unsigned char TDS_TINYINT;			/*  8-bit unsigned */
typedef tds_sysdep_int16_type TDS_SMALLINT;		/* 16-bit int      */
typedef unsigned tds_sysdep_int16_type TDS_USMALLINT;	/* 16-bit unsigned */
typedef tds_sysdep_int32_type TDS_INT;			/* 32-bit int      */
typedef unsigned tds_sysdep_int32_type TDS_UINT;	/* 32-bit unsigned */
typedef tds_sysdep_real32_type TDS_REAL;		/* 32-bit real     */
typedef tds_sysdep_real64_type TDS_FLOAT;		/* 64-bit real     */
typedef tds_sysdep_int64_type TDS_INT8;			/* 64-bit integer  */
typedef unsigned tds_sysdep_int64_type TDS_UINT8;	/* 64-bit unsigned */
typedef tds_sysdep_intptr_type TDS_INTPTR;

typedef struct tdsnumeric
{
	unsigned char precision;
	unsigned char scale;
	unsigned char array[33];
} TDS_NUMERIC;

typedef struct tdsoldmoney
{
	TDS_INT mnyhigh;
	TDS_UINT mnylow;
} TDS_OLD_MONEY;

typedef union tdsmoney
{
	TDS_OLD_MONEY tdsoldmoney;
	TDS_INT8 mny;
} TDS_MONEY;

typedef struct tdsmoney4
{
	TDS_INT mny4;
} TDS_MONEY4;

typedef struct tdsdatetime
{
	TDS_INT dtdays;
	TDS_INT dttime;
} TDS_DATETIME;

typedef struct tdsdatetime4
{
	TDS_USMALLINT days;
	TDS_USMALLINT minutes;
} TDS_DATETIME4;

typedef struct tdsvarbinary
{
	TDS_SMALLINT len;
	TDS_CHAR array[256];
} TDS_VARBINARY;
typedef struct tdsvarchar
{
	TDS_SMALLINT len;
	TDS_CHAR array[256];
} TDS_VARCHAR;

typedef struct tdsunique
{
	TDS_UINT Data1;
	TDS_USMALLINT Data2;
	TDS_USMALLINT Data3;
	TDS_UCHAR Data4[8];
} TDS_UNIQUE;

/** Used by tds_datecrack */
typedef struct tdsdaterec
{
	TDS_INT year;	       /**< year */
	TDS_INT quarter;       /**< quarter (0-3) */
	TDS_INT month;	       /**< month number (0-11) */
	TDS_INT day;	       /**< day of month (1-31) */
	TDS_INT dayofyear;     /**< day of year  (1-366) */
	TDS_INT week;          /**< 1 - 54 (can be 54 in leap year) */
	TDS_INT weekday;       /**< day of week  (0-6, 0 = sunday) */
	TDS_INT hour;	       /**< 0-23 */
	TDS_INT minute;	       /**< 0-59 */
	TDS_INT second;	       /**< 0-59 */
	TDS_INT millisecond;   /**< 0-999 */
	TDS_INT tzone;
} TDSDATEREC;

/**
 * The following little table is indexed by precision and will
 * tell us the number of bytes required to store the specified
 * precision.
 */
extern const int tds_numeric_bytes_per_prec[];

#define TDS_SUCCEED          1
#define TDS_FAIL             0
#define TDS_NO_MORE_RESULTS  2
#define TDS_CANCELLED        3

#define TDS_INT_CONTINUE 1
#define TDS_INT_CANCEL 2
#define TDS_INT_TIMEOUT 3


#define TDS_NO_COUNT         -1

#define TDS_ROW_RESULT        4040
#define TDS_PARAM_RESULT      4042
#define TDS_STATUS_RESULT     4043
#define TDS_MSG_RESULT        4044
#define TDS_COMPUTE_RESULT    4045
#define TDS_CMD_DONE          4046
#define TDS_CMD_SUCCEED       4047
#define TDS_CMD_FAIL          4048
#define TDS_ROWFMT_RESULT     4049
#define TDS_COMPUTEFMT_RESULT 4050
#define TDS_DESCRIBE_RESULT   4051
#define TDS_DONE_RESULT       4052
#define TDS_DONEPROC_RESULT   4053
#define TDS_DONEINPROC_RESULT 4054
#define TDS_OTHERS_RESULT     4055

enum tds_token_results
{
	TDS_TOKEN_RES_OTHERS,
	TDS_TOKEN_RES_ROWFMT,
	TDS_TOKEN_RES_COMPUTEFMT,
	TDS_TOKEN_RES_PARAMFMT,
	TDS_TOKEN_RES_DONE,
	TDS_TOKEN_RES_ROW,
	TDS_TOKEN_RES_COMPUTE,
	TDS_TOKEN_RES_PROC,
	TDS_TOKEN_RES_MSG
};

#define TDS_TOKEN_FLAG(flag) TDS_RETURN_##flag = (1 << (TDS_TOKEN_RES_##flag*2)), TDS_STOPAT_##flag = (2 << (TDS_TOKEN_RES_##flag*2))

enum tds_token_flags
{
	TDS_HANDLE_ALL = 0,
	TDS_TOKEN_FLAG(OTHERS),
	TDS_TOKEN_FLAG(ROWFMT),
	TDS_TOKEN_FLAG(COMPUTEFMT),
	TDS_TOKEN_FLAG(PARAMFMT),
	TDS_TOKEN_FLAG(DONE),
	TDS_TOKEN_FLAG(ROW),
	TDS_TOKEN_FLAG(COMPUTE),
	TDS_TOKEN_FLAG(PROC),
	TDS_TOKEN_FLAG(MSG),
	TDS_TOKEN_RESULTS = TDS_RETURN_ROWFMT|TDS_RETURN_COMPUTEFMT|TDS_RETURN_DONE|TDS_STOPAT_ROW|TDS_STOPAT_COMPUTE|TDS_RETURN_PROC,
	TDS_TOKEN_TRAILING = TDS_STOPAT_ROWFMT|TDS_STOPAT_COMPUTEFMT|TDS_STOPAT_ROW|TDS_STOPAT_COMPUTE|TDS_STOPAT_MSG|TDS_STOPAT_OTHERS
};

/**
 * Flags returned in TDS_DONE token
 */
enum tds_end
{
	  TDS_DONE_FINAL 	= 0x00	/**< final result set, command completed successfully. */
	, TDS_DONE_MORE_RESULTS = 0x01	/**< more results follow */
	, TDS_DONE_ERROR 	= 0x02	/**< error occurred */
	, TDS_DONE_INXACT 	= 0x04	/**< transaction in progress */
	, TDS_DONE_PROC 	= 0x08	/**< results are from a stored procedure */
	, TDS_DONE_COUNT 	= 0x10	/**< count field in packet is valid */
	, TDS_DONE_CANCELLED 	= 0x20	/**< acknowledging an attention command (usually a cancel) */
	, TDS_DONE_EVENT 	= 0x40	/*   part of an event notification. */
	, TDS_DONE_SRVERROR 	= 0x100	/**< SQL server server error */
	
	/* after the above flags, a TDS_DONE packet has a field describing the state of the transaction */
	, TDS_DONE_NO_TRAN 	= 0	/* No transaction in effect */
	, TDS_DONE_TRAN_SUCCEED = 1	/* Transaction completed successfully */
	, TDS_DONE_TRAN_PROGRESS= 2	/* Transaction in progress */
	, TDS_DONE_STMT_ABORT 	= 3	/* A statement aborted */
	, TDS_DONE_TRAN_ABORT 	= 4	/* Transaction aborted */
};


/*
 * TDSERRNO is emitted by libtds to the client library's error handler
 * (which may in turn call the client's error handler).
 * These match the db-lib msgno, because the same values have the same meaning
 * in db-lib and ODBC.  ct-lib maps them to ct-lib numbers (todo). 
 */
typedef enum {	TDSEOK    = TDS_SUCCEED, 
		TDSEVERDOWN    =  100,
		TDSEICONVIU    = 2400, 
		TDSEICONVAVAIL = 2401, 
		TDSEICONVO     = 2402, 
		TDSEICONVI     = 2403, 
		TDSEICONV2BIG  = 2404,
		TDSEPORTINSTANCE	= 2500,
		TDSESYNC = 20001, 
		TDSEFCON = 20002, 
		TDSETIME = 20003, 
		TDSEREAD = 20004, 
		TDSEWRIT = 20006, 
		TDSESOCK = 20008, 
		TDSECONN = 20009, 
		TDSEMEM  = 20010,
		TDSEINTF = 20012,	/* Server name not found in interface file */
		TDSEUHST = 20013,	/* Unknown host machine name. */
		TDSEPWD  = 20014, 
		TDSESEOF = 20017, 
		TDSERPND = 20019, 
		TDSEBTOK = 20020, 
		TDSEOOB  = 20022, 
		TDSECLOS = 20056,
		TDSEUSCT = 20058, 
		TDSEUTDS = 20146, 
		TDSEEUNR = 20185, 
		TDSECAP  = 20203, 
		TDSENEG  = 20210, 
		TDSEUMSG = 20212, 
		TDSECAPTYP  = 20213, 
		TDSEBPROBADTYP = 20250,
		TDSECLOSEIN = 20292 
} TDSERRNO;

#define TDS5_PARAMFMT2_TOKEN       32	/* 0x20 */
#define TDS_LANGUAGE_TOKEN         33	/* 0x21    TDS 5.0 only              */
#define TDS_ORDERBY2_TOKEN         34	/* 0x22 */
#define TDS_ROWFMT2_TOKEN          97	/* 0x61    TDS 5.0 only              */
#define TDS_LOGOUT_TOKEN          113	/* 0x71    TDS 5.0 only? ct_close()  */
#define TDS_RETURNSTATUS_TOKEN    121	/* 0x79                              */
#define TDS_PROCID_TOKEN          124	/* 0x7C    TDS 4.2 only - TDS_PROCID */
#define TDS7_RESULT_TOKEN         129	/* 0x81    TDS 7.0 only              */
#define TDS7_COMPUTE_RESULT_TOKEN 136	/* 0x88    TDS 7.0 only              */
#define TDS_COLNAME_TOKEN         160	/* 0xA0    TDS 4.2 only              */
#define TDS_COLFMT_TOKEN          161	/* 0xA1    TDS 4.2 only - TDS_COLFMT */
#define TDS_DYNAMIC2_TOKEN        163	/* 0xA3 */
#define TDS_TABNAME_TOKEN         164	/* 0xA4 */
#define TDS_COLINFO_TOKEN         165	/* 0xA5 */
#define TDS_OPTIONCMD_TOKEN   	  166	/* 0xA6 */
#define TDS_COMPUTE_NAMES_TOKEN   167	/* 0xA7 */
#define TDS_COMPUTE_RESULT_TOKEN  168	/* 0xA8 */
#define TDS_ORDERBY_TOKEN         169	/* 0xA9    TDS_ORDER                 */
#define TDS_ERROR_TOKEN           170	/* 0xAA                              */
#define TDS_INFO_TOKEN            171	/* 0xAB                              */
#define TDS_PARAM_TOKEN           172	/* 0xAC    RETURNVALUE?              */
#define TDS_LOGINACK_TOKEN        173	/* 0xAD                              */
#define TDS_CONTROL_TOKEN         174	/* 0xAE    TDS_CONTROL               */
#define TDS_ROW_TOKEN             209	/* 0xD1                              */
#define TDS_NBC_ROW_TOKEN         210	/* 0xD2    as of TDS 7.3.B           */ /* not implemented */
#define TDS_CMP_ROW_TOKEN         211	/* 0xD3                              */
#define TDS5_PARAMS_TOKEN         215	/* 0xD7    TDS 5.0 only              */
#define TDS_CAPABILITY_TOKEN      226	/* 0xE2                              */
#define TDS_ENVCHANGE_TOKEN       227	/* 0xE3                              */
#define TDS_EED_TOKEN             229	/* 0xE5                              */
#define TDS_DBRPC_TOKEN           230	/* 0xE6                              */
#define TDS5_DYNAMIC_TOKEN        231	/* 0xE7    TDS 5.0 only              */
#define TDS5_PARAMFMT_TOKEN       236	/* 0xEC    TDS 5.0 only              */
#define TDS_AUTH_TOKEN            237	/* 0xED    TDS 7.0 only              */
#define TDS_RESULT_TOKEN          238	/* 0xEE                              */
#define TDS_DONE_TOKEN            253	/* 0xFD    TDS_DONE                  */
#define TDS_DONEPROC_TOKEN        254	/* 0xFE    TDS_DONEPROC              */
#define TDS_DONEINPROC_TOKEN      255	/* 0xFF    TDS_DONEINPROC            */

/* CURSOR support: TDS 5.0 only*/
#define TDS_CURCLOSE_TOKEN        128  /* 0x80    TDS 5.0 only              */
#define TDS_CURDELETE_TOKEN       129  /* 0x81    TDS 5.0 only              */
#define TDS_CURFETCH_TOKEN        130  /* 0x82    TDS 5.0 only              */
#define TDS_CURINFO_TOKEN         131  /* 0x83    TDS 5.0 only              */
#define TDS_CUROPEN_TOKEN         132  /* 0x84    TDS 5.0 only              */
#define TDS_CURDECLARE_TOKEN      134  /* 0x86    TDS 5.0 only              */

enum {
	TDS_CUR_ISTAT_UNUSED    = 0x00,
	TDS_CUR_ISTAT_DECLARED  = 0x01,
	TDS_CUR_ISTAT_OPEN      = 0x02,
	TDS_CUR_ISTAT_CLOSED    = 0x04,
	TDS_CUR_ISTAT_RDONLY    = 0x08,
	TDS_CUR_ISTAT_UPDATABLE = 0x10,
	TDS_CUR_ISTAT_ROWCNT    = 0x20,
	TDS_CUR_ISTAT_DEALLOC   = 0x40
};

/* http://jtds.sourceforge.net/apiCursors.html */
/* Cursor scroll option, must be one of 0x01 - 0x10, OR'd with other bits */
enum {
	TDS_CUR_TYPE_KEYSET          = 0x0001, /* default */
	TDS_CUR_TYPE_DYNAMIC         = 0x0002,
	TDS_CUR_TYPE_FORWARD         = 0x0004,
	TDS_CUR_TYPE_STATIC          = 0x0008,
	TDS_CUR_TYPE_FASTFORWARDONLY = 0x0010,
	TDS_CUR_TYPE_PARAMETERIZED   = 0x1000,
	TDS_CUR_TYPE_AUTO_FETCH      = 0x2000
};

enum {
	TDS_CUR_CONCUR_READ_ONLY         = 1,
	TDS_CUR_CONCUR_SCROLL_LOCKS      = 2,
	TDS_CUR_CONCUR_OPTIMISTIC        = 4, /* default */
	TDS_CUR_CONCUR_OPTIMISTIC_VALUES = 8
};

/* TDS 4/5 login*/
#define TDS_MAXNAME 30	/* maximum login name lenghts */
#define TDS_PROGNLEN 10	/* maximum program lenght */
#define TDS_PKTLEN 6	/* maximum packet lenght in login */

/* environment type field */
#define TDS_ENV_DATABASE  	1
#define TDS_ENV_LANG      	2
#define TDS_ENV_CHARSET   	3
#define TDS_ENV_PACKSIZE  	4
#define TDS_ENV_LCID        	5
#define TDS_ENV_SQLCOLLATION	7
#define TDS_ENV_BEGINTRANS	8
#define TDS_ENV_COMMITTRANS	9
#define TDS_ENV_ROLLBACKTRANS	10

/* string types */
#define TDS_NULLTERM -9

/* Microsoft internal stored procedure id's */

#define TDS_SP_CURSOR           1
#define TDS_SP_CURSOROPEN       2
#define TDS_SP_CURSORPREPARE    3
#define TDS_SP_CURSOREXECUTE    4
#define TDS_SP_CURSORPREPEXEC   5
#define TDS_SP_CURSORUNPREPARE  6
#define TDS_SP_CURSORFETCH      7
#define TDS_SP_CURSOROPTION     8
#define TDS_SP_CURSORCLOSE      9
#define TDS_SP_EXECUTESQL      10
#define TDS_SP_PREPARE         11
#define TDS_SP_EXECUTE         12
#define TDS_SP_PREPEXEC        13
#define TDS_SP_PREPEXECRPC     14
#define TDS_SP_UNPREPARE       15
/* 
 * <rant> Sybase does an awful job of this stuff, non null ints of size 1 2 
 * and 4 have there own codes but nullable ints are lumped into INTN
 * sheesh! </rant>
 */
typedef enum
{
	SYBCHAR = 47,		/* 0x2F */
#define SYBCHAR	SYBCHAR
	SYBVARCHAR = 39,	/* 0x27 */
#define SYBVARCHAR	SYBVARCHAR
	SYBINTN = 38,		/* 0x26 */
#define SYBINTN	SYBINTN
	SYBINT1 = 48,		/* 0x30 */
#define SYBINT1	SYBINT1
	SYBINT2 = 52,		/* 0x34 */
#define SYBINT2	SYBINT2
	SYBINT4 = 56,		/* 0x38 */
#define SYBINT4	SYBINT4
	SYBFLT8 = 62,		/* 0x3E */
#define SYBFLT8	SYBFLT8
	SYBDATETIME = 61,	/* 0x3D */
#define SYBDATETIME	SYBDATETIME
	SYBBIT = 50,		/* 0x32 */
#define SYBBIT	SYBBIT
	SYBTEXT = 35,		/* 0x23 */
#define SYBTEXT	SYBTEXT
	SYBNTEXT = 99,		/* 0x63 */
#define SYBNTEXT	SYBNTEXT
	SYBIMAGE = 34,		/* 0x22 */
#define SYBIMAGE	SYBIMAGE
	SYBMONEY4 = 122,	/* 0x7A */
#define SYBMONEY4	SYBMONEY4
	SYBMONEY = 60,		/* 0x3C */
#define SYBMONEY	SYBMONEY
	SYBDATETIME4 = 58,	/* 0x3A */
#define SYBDATETIME4	SYBDATETIME4
	SYBREAL = 59,		/* 0x3B */
#define SYBREAL	SYBREAL
	SYBBINARY = 45,		/* 0x2D */
#define SYBBINARY	SYBBINARY
	SYBVOID = 31,		/* 0x1F */
#define SYBVOID	SYBVOID
	SYBVARBINARY = 37,	/* 0x25 */
#define SYBVARBINARY	SYBVARBINARY
	SYBBITN = 104,		/* 0x68 */
#define SYBBITN	SYBBITN
	SYBNUMERIC = 108,	/* 0x6C */
#define SYBNUMERIC	SYBNUMERIC
	SYBDECIMAL = 106,	/* 0x6A */
#define SYBDECIMAL	SYBDECIMAL
	SYBFLTN = 109,		/* 0x6D */
#define SYBFLTN	SYBFLTN
	SYBMONEYN = 110,	/* 0x6E */
#define SYBMONEYN	SYBMONEYN
	SYBDATETIMN = 111,	/* 0x6F */
#define SYBDATETIMN	SYBDATETIMN

/*
 * MS only types
 */
	SYBNVARCHAR = 103,	/* 0x67 */
#define SYBNVARCHAR	SYBNVARCHAR
	SYBINT8 = 127,		/* 0x7F */
#define SYBINT8	SYBINT8
	XSYBCHAR = 175,		/* 0xAF */
#define XSYBCHAR	XSYBCHAR
	XSYBVARCHAR = 167,	/* 0xA7 */
#define XSYBVARCHAR	XSYBVARCHAR
	XSYBNVARCHAR = 231,	/* 0xE7 */
#define XSYBNVARCHAR	XSYBNVARCHAR
	XSYBNCHAR = 239,	/* 0xEF */
#define XSYBNCHAR	XSYBNCHAR
	XSYBVARBINARY = 165,	/* 0xA5 */
#define XSYBVARBINARY	XSYBVARBINARY
	XSYBBINARY = 173,	/* 0xAD */
#define XSYBBINARY	XSYBBINARY
	SYBUNIQUE = 36,		/* 0x24 */
#define SYBUNIQUE	SYBUNIQUE
	SYBVARIANT = 98, 	/* 0x62 */
#define SYBVARIANT	SYBVARIANT
	SYBMSUDT = 240,		/* 0xF0 */
#define SYBMSUDT SYBMSUDT
	SYBMSXML = 241,		/* 0xF1 */
#define SYBMSXML SYBMSXML

/*
 * Sybase only types
 */
	SYBLONGBINARY = 225,	/* 0xE1 */
#define SYBLONGBINARY	SYBLONGBINARY
	SYBUINT1 = 64,		/* 0x40 */
#define SYBUINT1	SYBUINT1
	SYBUINT2 = 65,		/* 0x41 */
#define SYBUINT2	SYBUINT2
	SYBUINT4 = 66,		/* 0x42 */
#define SYBUINT4	SYBUINT4
	SYBUINT8 = 67,		/* 0x43 */
#define SYBUINT8	SYBUINT8
	SYBBLOB = 36,		/* 0x24 */
#define SYBBLOB		SYBBLOB
	SYBBOUNDARY = 104,	/* 0x68 */
#define SYBBOUNDARY	SYBBOUNDARY
	SYBDATE = 49,		/* 0x31 */
#define SYBDATE		SYBDATE
	SYBDATEN = 123,		/* 0x7B */
#define SYBDATEN	SYBDATEN
	SYB5INT8 = 191,		/* 0xBF */
#define SYB5INT8		SYB5INT8
	SYBINTERVAL = 46,	/* 0x2E */
#define SYBINTERVAL	SYBINTERVAL
	SYBLONGCHAR = 175,	/* 0xAF */
#define SYBLONGCHAR	SYBLONGCHAR
	SYBSENSITIVITY = 103,	/* 0x67 */
#define SYBSENSITIVITY	SYBSENSITIVITY
	SYBSINT1 = 176,		/* 0xB0 */
#define SYBSINT1	SYBSINT1
	SYBTIME = 51,		/* 0x33 */
#define SYBTIME		SYBTIME
	SYBTIMEN = 147,		/* 0x93 */
#define SYBTIMEN	SYBTIMEN
	SYBUINTN = 68,		/* 0x44 */
#define SYBUINTN	SYBUINTN
	SYBUNITEXT = 174,	/* 0xAE */
#define SYBUNITEXT	SYBUNITEXT
	SYBXML = 163,		/* 0xA3 */
#define SYBXML		SYBXML

} TDS_SERVER_TYPE;


typedef enum
{
	USER_UNICHAR_TYPE = 34,		/* 0x22 */
	USER_UNIVARCHAR_TYPE = 35	/* 0x23 */
} TDS_USER_TYPE;

#define SYBAOPCNT  0x4b
#define SYBAOPCNTU 0x4c
#define SYBAOPSUM  0x4d
#define SYBAOPSUMU 0x4e
#define SYBAOPAVG  0x4f
#define SYBAOPAVGU 0x50
#define SYBAOPMIN  0x51
#define SYBAOPMAX  0x52

/* mssql2k compute operator */
#define SYBAOPCNT_BIG		0x09
#define SYBAOPSTDEV		0x30
#define SYBAOPSTDEVP		0x31
#define SYBAOPVAR		0x32
#define SYBAOPVARP		0x33
#define SYBAOPCHECKSUM_AGG	0x72


/** 
 * options that can be sent with a TDS_OPTIONCMD token
 */
typedef enum
{
	  TDS_OPT_SET = 1	/* Set an option. */
	, TDS_OPT_DEFAULT = 2	/* Set option to its default value. */
	, TDS_OPT_LIST = 3	/* Request current setting of a specific option. */
	, TDS_OPT_INFO = 4	/* Report current setting of a specific option. */
} TDS_OPTION_CMD;

typedef enum
{
	  TDS_OPT_DATEFIRST = 1		/* 0x01 */
	, TDS_OPT_TEXTSIZE = 2		/* 0x02 */
	, TDS_OPT_STAT_TIME = 3		/* 0x03 */
	, TDS_OPT_STAT_IO = 4		/* 0x04 */
	, TDS_OPT_ROWCOUNT = 5		/* 0x05 */
	, TDS_OPT_NATLANG = 6		/* 0x06 */
	, TDS_OPT_DATEFORMAT = 7	/* 0x07 */
	, TDS_OPT_ISOLATION = 8		/* 0x08 */
	, TDS_OPT_AUTHON = 9		/* 0x09 */
	, TDS_OPT_CHARSET = 10		/* 0x0a */
	, TDS_OPT_SHOWPLAN = 13		/* 0x0d */
	, TDS_OPT_NOEXEC = 14		/* 0x0e */
	, TDS_OPT_ARITHIGNOREON = 15	/* 0x0f */
	, TDS_OPT_ARITHABORTON = 17	/* 0x11 */
	, TDS_OPT_PARSEONLY = 18	/* 0x12 */
	, TDS_OPT_GETDATA = 20		/* 0x14 */
	, TDS_OPT_NOCOUNT = 21		/* 0x15 */
	, TDS_OPT_FORCEPLAN = 23	/* 0x17 */
	, TDS_OPT_FORMATONLY = 24	/* 0x18 */
	, TDS_OPT_CHAINXACTS = 25	/* 0x19 */
	, TDS_OPT_CURCLOSEONXACT = 26	/* 0x1a */
	, TDS_OPT_FIPSFLAG = 27		/* 0x1b */
	, TDS_OPT_RESTREES = 28		/* 0x1c */
	, TDS_OPT_IDENTITYON = 29	/* 0x1d */
	, TDS_OPT_CURREAD = 30		/* 0x1e */
	, TDS_OPT_CURWRITE = 31		/* 0x1f */
	, TDS_OPT_IDENTITYOFF = 32	/* 0x20 */
	, TDS_OPT_AUTHOFF = 33		/* 0x21 */
	, TDS_OPT_ANSINULL = 34		/* 0x22 */
	, TDS_OPT_QUOTED_IDENT = 35	/* 0x23 */
	, TDS_OPT_ARITHIGNOREOFF = 36	/* 0x24 */
	, TDS_OPT_ARITHABORTOFF = 37	/* 0x25 */
	, TDS_OPT_TRUNCABORT = 38	/* 0x26 */
} TDS_OPTION;

typedef union tds_option_arg
{
	TDS_TINYINT ti;
	TDS_INT i;
	TDS_CHAR *c;
} TDS_OPTION_ARG;

enum {
	TDS_OPT_ARITHOVERFLOW = 0x01,
	TDS_OPT_NUMERICTRUNC = 0x02
};

enum TDS_OPT_DATEFIRST_CHOICE
{
	TDS_OPT_MONDAY = 1, TDS_OPT_TUESDAY = 2, TDS_OPT_WEDNESDAY = 3, TDS_OPT_THURSDAY = 4, TDS_OPT_FRIDAY = 5, TDS_OPT_SATURDAY =
		6, TDS_OPT_SUNDAY = 7
};

enum TDS_OPT_DATEFORMAT_CHOICE
{
	TDS_OPT_FMTMDY = 1, TDS_OPT_FMTDMY = 2, TDS_OPT_FMTYMD = 3, TDS_OPT_FMTYDM = 4, TDS_OPT_FMTMYD = 5, TDS_OPT_FMTDYM = 6
};
enum TDS_OPT_ISOLATION_CHOICE
{
	TDS_OPT_LEVEL1 = 1, TDS_OPT_LEVEL3 = 3
};

typedef enum tds_packet_type
{
	TDS_QUERY = 1,
	TDS_LOGIN = 2,
	TDS_RPC = 3,
	TDS_REPLY = 4,
	TDS_CANCEL = 6,
	TDS_BULK = 7,
	TDS_NORMAL = 15,
	TDS7_LOGIN = 16,
	TDS7_AUTH = 17,
	TDS8_PRELOGIN = 18
} TDS_PACKET_TYPE;

typedef enum tds_encryption_level {
	TDS_ENCRYPTION_OFF, TDS_ENCRYPTION_REQUEST, TDS_ENCRYPTION_REQUIRE
} TDS_ENCRYPTION_LEVEL;

#define TDS_ZERO_FREE(x) do {free((x)); (x) = NULL;} while(0)
#define TDS_VECTOR_SIZE(x) (sizeof(x)/sizeof(x[0]))

#if defined(__GNUC__) && __GNUC__ >= 3
# define TDS_LIKELY(x)	__builtin_expect(!!(x), 1)
# define TDS_UNLIKELY(x)	__builtin_expect(!!(x), 0)
#else
# define TDS_LIKELY(x)	(x)
# define TDS_UNLIKELY(x)	(x)
#endif

/*
 * TODO use system macros for optimization
 * See mcrypt for reference and linux kernel source for optimization
 * check if unaligned access and use fast write/read when implemented
 */
#define TDS_BYTE_SWAP16(value)                 \
         (((((unsigned short)value)<<8) & 0xFF00)   | \
          ((((unsigned short)value)>>8) & 0x00FF))

#define TDS_BYTE_SWAP32(value)                     \
         (((((unsigned long)value)<<24) & 0xFF000000)  | \
          ((((unsigned long)value)<< 8) & 0x00FF0000)  | \
          ((((unsigned long)value)>> 8) & 0x0000FF00)  | \
          ((((unsigned long)value)>>24) & 0x000000FF))

#define is_end_token(x) (x==TDS_DONE_TOKEN    || \
			x==TDS_DONEPROC_TOKEN    || \
			x==TDS_DONEINPROC_TOKEN)

#define is_hard_end_token(x) (x==TDS_DONE_TOKEN    || \
			x==TDS_DONEPROC_TOKEN)

#define is_msg_token(x) (x==TDS_INFO_TOKEN    || \
			x==TDS_ERROR_TOKEN    || \
			x==TDS_EED_TOKEN)

#define is_result_token(x) (x==TDS_RESULT_TOKEN || \
			x==TDS_ROWFMT2_TOKEN    || \
			x==TDS7_RESULT_TOKEN    || \
			x==TDS_COLFMT_TOKEN     || \
			x==TDS_COLNAME_TOKEN    || \
			x==TDS_RETURNSTATUS_TOKEN)

/* FIXME -- not a complete list */
#define is_fixed_type(x) (x==SYBINT1    || \
			x==SYBINT2      || \
			x==SYBINT4      || \
			x==SYBINT8      || \
			x==SYBREAL      || \
			x==SYBFLT8      || \
			x==SYBDATETIME  || \
			x==SYBDATETIME4 || \
			x==SYBBIT       || \
			x==SYBMONEY     || \
			x==SYBMONEY4    || \
			x==SYBVOID      || \
			x==SYBUNIQUE)
#define is_nullable_type(x) ( \
		     x==SYBBITN      || \
                     x==SYBINTN      || \
                     x==SYBFLTN      || \
                     x==SYBMONEYN    || \
                     x==SYBDATETIMN  || \
                     x==SYBVARCHAR   || \
                     x==SYBVARBINARY || \
                     x==SYBTEXT      || \
                     x==SYBNTEXT     || \
                     x==SYBIMAGE)

#define is_variable_type(x) ( \
	(x)==SYBTEXT	|| \
	(x)==SYBIMAGE	|| \
	(x)==SYBNTEXT	|| \
	(x)==SYBCHAR	|| \
	(x)==SYBVARCHAR	|| \
	(x)==SYBBINARY	|| \
	(x)==SYBVARBINARY	|| \
	(x)==SYBLONGBINARY	|| \
	(x)==XSYBCHAR	|| \
	(x)==XSYBVARCHAR	|| \
	(x)==XSYBNVARCHAR	|| \
	(x)==XSYBNCHAR)

#define is_blob_type(x) (x==SYBTEXT || x==SYBIMAGE || x==SYBNTEXT)
#define is_blob_col(x) ((x)->column_varint_size > 2)
/* large type means it has a two byte size field */
/* define is_large_type(x) (x>128) */
#define is_numeric_type(x) (x==SYBNUMERIC || x==SYBDECIMAL)
#define is_unicode_type(x) (x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT || x==SYBMSXML)
#define is_collate_type(x) (x==XSYBVARCHAR || x==XSYBCHAR || x==SYBTEXT || x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_ascii_type(x) ( x==XSYBCHAR || x==XSYBVARCHAR || x==SYBTEXT || x==SYBCHAR || x==SYBVARCHAR)
#define is_char_type(x) (is_unicode_type(x) || is_ascii_type(x))
#define is_similar_type(x, y) ((is_char_type(x) && is_char_type(y)) || ((is_unicode_type(x) && is_unicode_type(y))))


#define TDS_MAX_CAPABILITY	22
#define MAXPRECISION 		77
#define TDS_MAX_CONN		4096
#define TDS_MAX_DYNID_LEN	30

/* defaults to use if no others are found */
#define TDS_DEF_SERVER		"SYBASE"
#define TDS_DEF_BLKSZ		512
#define TDS_DEF_CHARSET		"iso_1"
#define TDS_DEF_LANG		"us_english"
#if TDS42
#define TDS_DEFAULT_VERSION	0x402
#define TDS_DEF_PORT		1433
#elif TDS46
#define TDS_DEFAULT_VERSION	0x406
#define TDS_DEF_PORT		4000
#elif TDS70
#define TDS_DEFAULT_VERSION	0x700
#define TDS_DEF_PORT		1433
#elif TDS71
#define TDS_DEFAULT_VERSION	0x701
#define TDS_DEF_PORT		1433
#elif TDS72
#define TDS_DEFAULT_VERSION	0x702
#define TDS_DEF_PORT		1433
#else
#define TDS_DEFAULT_VERSION	0x500
#define TDS_DEF_PORT		4000
#endif

/* normalized strings from freetds.conf file */
#define TDS_STR_VERSION  "tds version"
#define TDS_STR_BLKSZ    "initial block size"
#define TDS_STR_SWAPDT   "swap broken dates"
#define TDS_STR_DUMPFILE "dump file"
#define TDS_STR_DEBUGLVL "debug level"
#define TDS_STR_DEBUGFLAGS "debug flags"
#define TDS_STR_TIMEOUT  "timeout"
#define TDS_STR_QUERY_TIMEOUT  "query timeout"
#define TDS_STR_CONNTIMEOUT "connect timeout"
#define TDS_STR_HOSTNAME "hostname"
#define TDS_STR_HOST     "host"
#define TDS_STR_PORT     "port"
#define TDS_STR_TEXTSZ   "text size"
/* for big endian hosts */
#define TDS_STR_EMUL_LE	"emulate little endian"
#define TDS_STR_CHARSET	"charset"
#define TDS_STR_CLCHARSET	"client charset"
#define TDS_STR_LANGUAGE	"language"
#define TDS_STR_APPENDMODE	"dump file append"
#define TDS_STR_DATEFMT	"date format"
#define TDS_STR_INSTANCE "instance"
#define TDS_STR_ASA_DATABASE	"asa database"
#define TDS_STR_ENCRYPTION	 "encryption"
#define TDS_STR_USENTLMV2	"use ntlmv2"
/* conf values */
#define TDS_STR_ENCRYPTION_OFF	 "off"
#define TDS_STR_ENCRYPTION_REQUEST "request"
#define TDS_STR_ENCRYPTION_REQUIRE "require"
/* Defines to enable optional GSSAPI delegation */
#define TDS_GSSAPI_DELEGATION "enable gssapi delegation"
/* Kerberos realm name */
#define TDS_STR_REALM	"realm"
/* Application Intent MSSQL 2012 support*/
#define TDS_STR_APPLICATION_INTENT "application intent"


/* TODO do a better check for alignment than this */
typedef union
{
	void *p;
	int i;
} tds_align_struct;

#define TDS_ALIGN_SIZE sizeof(tds_align_struct)

#define TDS_MAX_LOGIN_STR_SZ 128
typedef struct tds_login
{
	DSTR server_name;
	int port;
	TDS_USMALLINT tds_version;	/* TDS version */
	int block_size;
	DSTR language;			/* e.g. us-english */
	DSTR server_charset;		/* e.g. iso_1 */
	TDS_INT connect_timeout;
	DSTR client_host_name;
	DSTR app_name;
	DSTR user_name;
	DSTR password;
	
	DSTR library;	/* Ct-Library, DB-Library,  TDS-Library or ODBC */
	TDS_TINYINT encryption_level;

	TDS_INT query_timeout;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	DSTR client_charset;
	DSTR database;
	unsigned int bulk_copy:1;
	unsigned int suppress_language:1;
        unsigned int application_intent:1;
} TDSLOGIN;

typedef struct tds_connection
{
	/* first part of structure is the same of login one */
	DSTR server_name; /**< server name (in freetds.conf) */
	int port;	   /**< port of database service */
	TDS_USMALLINT tds_version;
	int block_size;
	DSTR language;
	DSTR server_charset;	/**< charset of server */
	TDS_INT connect_timeout;
	DSTR client_host_name;
	DSTR server_host_name;
	DSTR server_realm_name;		/**< server realm name (in freetds.conf) */
	DSTR app_name;
	DSTR user_name;	    	/**< account for login */
	DSTR password;	    	/**< password of account login */
	DSTR library;
	TDS_TINYINT encryption_level;

	TDS_INT query_timeout;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	unsigned char option_flag2;
	DSTR client_charset;

	DSTR ip_addr;	  	/**< ip of server */
	DSTR instance_name;
	DSTR database;
	DSTR dump_file;
	int debug_flags;
	int text_size;
	unsigned int broken_dates:1;
	unsigned int emul_little_endian:1;
	unsigned int bulk_copy:1;
	unsigned int suppress_language:1;
	unsigned int gssapi_use_delegation:1;
	unsigned int use_ntlmv2:1;
  	unsigned int application_intent:1;
} TDSCONNECTION;

typedef struct tds_locale
{
	char *language;
	char *server_charset;
	char *date_fmt;
} TDSLOCALE;

/** 
 * Information about blobs (e.g. text or image).
 * current_row contains this structure.
 */
typedef struct tds_blob
{
	TDS_CHAR *textvalue;
	TDS_CHAR textptr[16];
	TDS_CHAR timestamp[8];
} TDSBLOB;

/**
 * Store variant informations
 */
typedef struct tds_variant
{
	/* this MUST have same position and place of textvalue in tds_blob */
	TDS_CHAR *data;
	TDS_INT size;
	TDS_INT data_len;
	TDS_UCHAR type;
	TDS_UCHAR collation[5];
} TDSVARIANT;

/** 
 * TDS 8.0 collation informations.
 */
typedef struct
{
	TDS_USMALLINT locale_id;	/* master..syslanguages.lcid */
	TDS_USMALLINT flags;
	TDS_UCHAR charset_id;		/* or zero */
} TDS8_COLLATION;

/* SF stands for "sort flag" */
#define TDS_SF_BIN                   (TDS_USMALLINT) 0x100
#define TDS_SF_WIDTH_INSENSITIVE     (TDS_USMALLINT) 0x080
#define TDS_SF_KATATYPE_INSENSITIVE  (TDS_USMALLINT) 0x040
#define TDS_SF_ACCENT_SENSITIVE      (TDS_USMALLINT) 0x020
#define TDS_SF_CASE_INSENSITIVE      (TDS_USMALLINT) 0x010

/* UT stands for user type */
#define TDS_UT_TIMESTAMP             80


/**
 * Information relevant to libiconv.  The name is an iconv name, not 
 * the same as found in master..syslanguages. 
 */
typedef struct tds_encoding
{
	const char *name;
	unsigned char min_bytes_per_char;
	unsigned char max_bytes_per_char;
	unsigned char canonic;
} TDS_ENCODING;

typedef struct tds_bcpcoldata
{
	TDS_UCHAR *data;
	TDS_INT    datalen;
	TDS_INT    is_null;
} BCPCOLDATA;


enum
{ TDS_SYSNAME_SIZE = 512 };

/** 
 * Metadata about columns in regular and compute rows 
 */
typedef struct tds_column
{
	TDS_SMALLINT column_type;	/**< This type can be different from wire type because 
	 				 * conversion (e.g. UCS-2->Ascii) can be applied.    
					 * I'm beginning to wonder about the wisdom of this, however. 
					 * April 2003 jkl
					 */
	TDS_INT column_usertype;
	TDS_INT column_flags;

	TDS_INT column_size;		/**< maximun size of data. For fixed is the size. */

	TDS_TINYINT column_varint_size;	/**< size of length when reading from wire (0, 1, 2 or 4) */

	TDS_TINYINT column_prec;	/**< precision for decimal/numeric */
	TDS_TINYINT column_scale;	/**< scale for decimal/numeric */

	TDS_SMALLINT column_namelen;	/**< length of column name */
	TDS_SMALLINT table_namelen;
	struct
	{
		TDS_SMALLINT column_type;	/**< type of data, saved from wire */
		TDS_INT column_size;
	} on_server;

	TDSICONV *char_conv;	/**< refers to previously allocated iconv information */

	TDS_CHAR table_name[TDS_SYSNAME_SIZE];
	TDS_CHAR column_name[TDS_SYSNAME_SIZE];
	char * table_column_name;

	unsigned char *column_data;
	void (*column_data_free)(struct tds_column *column);
	unsigned int column_nullable:1;
	unsigned int column_writeable:1;
	unsigned int column_identity:1;
	unsigned int column_key:1;
	unsigned int column_hidden:1;
	unsigned int column_output:1;
	unsigned int column_timestamp:1;
	TDS_UCHAR column_collation[5];

	/* additional fields flags for compute results */
	TDS_TINYINT column_operator;
	TDS_SMALLINT column_operand;

	/* FIXME this is data related, not column */
	/** size written in variable (ie: char, text, binary). -1 if NULL. */
	TDS_INT column_cur_size;

	/* related to binding or info stored by client libraries */
	/* FIXME find a best place to store these data, some are unused */
	TDS_SMALLINT column_bindtype;
	TDS_SMALLINT column_bindfmt;
	TDS_UINT column_bindlen;
	TDS_SMALLINT *column_nullbind;
	TDS_CHAR *column_varaddr;
	TDS_INT *column_lenbind;
	TDS_INT column_textpos;
	TDS_INT column_text_sqlgetdatapos;
	TDS_CHAR column_text_sqlputdatainfo;

	BCPCOLDATA *bcp_column_data;
	/**
	 * The length, in bytes, of any length prefix this column may have.
	 * For example, strings in some non-C programming languages are
	 * made up of a one-byte length prefix, followed by the string
	 * data itself.
	 * If the data do not have a length prefix, set prefixlen to 0.
	 * Currently not very used in code, however do not remove.
	 */
	TDS_INT bcp_prefix_len;
	TDS_INT bcp_term_len;
	TDS_CHAR *bcp_terminator;
} TDSCOLUMN;


/** Hold information for any results */
typedef struct tds_result_info
{
	/* TODO those fields can became a struct */
	TDS_SMALLINT num_cols;
	TDSCOLUMN **columns;
	TDS_INT row_size;
	TDS_INT ref_count;
	unsigned char *current_row;
	void (*row_free)(struct tds_result_info* result, unsigned char *row);

	TDS_SMALLINT rows_exist;
	/* TODO remove ?? used only in dblib */
	TDS_INT row_count;
	/* TODO remove ?? used only in dblib */
	TDS_TINYINT more_results;
	TDS_SMALLINT computeid;
	TDS_SMALLINT *bycolumns;
	TDS_SMALLINT by_cols;
} TDSRESULTINFO;

/** values for tds->state */
typedef enum _TDS_STATE
{
	TDS_IDLE,	/**< no data expected */
	TDS_QUERYING,	/**< client is sending request */
	TDS_PENDING,	/**< cilent is waiting for data */
	TDS_READING,	/**< client is reading data */
	TDS_DEAD	/**< no connection */
} TDS_STATE;

#define TDS_DBG_LOGIN   __FILE__, ((__LINE__ << 4) | 11)
#define TDS_DBG_HEADER  __FILE__, ((__LINE__ << 4) | 10)
#define TDS_DBG_FUNC    __FILE__, ((__LINE__ << 4) |  7)
#define TDS_DBG_INFO2   __FILE__, ((__LINE__ << 4) |  6)
#define TDS_DBG_INFO1   __FILE__, ((__LINE__ << 4) |  5)
#define TDS_DBG_NETWORK __FILE__, ((__LINE__ << 4) |  4)
#define TDS_DBG_WARN    __FILE__, ((__LINE__ << 4) |  3)
#define TDS_DBG_ERROR   __FILE__, ((__LINE__ << 4) |  2)
#define TDS_DBG_SEVERE  __FILE__, ((__LINE__ << 4) |  1)

#define TDS_DBGFLAG_FUNC    0x80
#define TDS_DBGFLAG_INFO2   0x40
#define TDS_DBGFLAG_INFO1   0x20
#define TDS_DBGFLAG_NETWORK 0x10
#define TDS_DBGFLAG_WARN    0x08
#define TDS_DBGFLAG_ERROR   0x04
#define TDS_DBGFLAG_SEVERE  0x02
#define TDS_DBGFLAG_ALL     0xfff
#define TDS_DBGFLAG_LOGIN   0x0800
#define TDS_DBGFLAG_HEADER  0x0400
#define TDS_DBGFLAG_PID     0x1000
#define TDS_DBGFLAG_TIME    0x2000
#define TDS_DBGFLAG_SOURCE  0x4000
#define TDS_DBGFLAG_THREAD  0x8000

#if 0
/**
 * An attempt at better logging.
 * Using these bitmapped values, various logging features can be turned on and off.
 * It can be especially helpful to turn packet data on/off for security reasons.
 */
enum TDS_DBG_LOG_STATE
{
	  TDS_DBG_LOGIN =  (1 << 0)	/**< for diagnosing login problems;                                       
				 	   otherwise the username/password information is suppressed. */
	, TDS_DBG_API =    (1 << 1)	/**< Log calls to client libraries */
	, TDS_DBG_ASYNC =  (1 << 2)	/**< Log asynchronous function starts or completes. */
	, TDS_DBG_DIAG =   (1 << 3)	/**< Log client- and server-generated messages */
	, TDS_DBG_error =  (1 << 4)
	/* TODO:  ^^^^^ make upper case when old #defines (above) are removed */
	/* Log FreeTDS runtime/logic error occurs. */
	, TDS_DBG_PACKET = (1 << 5)	/**< Log hex dump of packets to/from the server. */
	, TDS_DBG_LIBTDS = (1 << 6)	/**< Log calls to (and in) libtds */
	, TDS_DBG_CONFIG = (1 << 7)	/**< replaces TDSDUMPCONFIG */
	, TDS_DBG_DEFAULT = 0xFE	/**< all above except login packets */
};
#endif

typedef struct tds_result_info TDSCOMPUTEINFO;

typedef TDSRESULTINFO TDSPARAMINFO;

typedef struct tds_message
{
	TDS_CHAR *server;
	TDS_CHAR *message;
	TDS_CHAR *proc_name;
	TDS_CHAR *sql_state;
	TDS_UINT msgno;
	TDS_INT line_number;
	/* -1 .. 255 */
	TDS_SMALLINT state;
	TDS_TINYINT priv_msg_type;
	TDS_TINYINT severity;
	/* for library-generated errors */
	int oserr;
} TDSMESSAGE;

typedef struct tds_upd_col
{
	struct tds_upd_col *next;	
	TDS_INT colnamelength;
	char * columnname;
} TDSUPDCOL;

typedef enum {
	  TDS_CURSOR_STATE_UNACTIONED = 0   	/* initial value */
	, TDS_CURSOR_STATE_REQUESTED = 1	/* called by ct_cursor */ 
	, TDS_CURSOR_STATE_SENT = 2		/* sent to server */
	, TDS_CURSOR_STATE_ACTIONED = 3		/* acknowledged by server */
} TDS_CURSOR_STATE;

typedef struct tds_cursor_status
{
	TDS_CURSOR_STATE declare;
	TDS_CURSOR_STATE cursor_row;
	TDS_CURSOR_STATE open;
	TDS_CURSOR_STATE fetch;
	TDS_CURSOR_STATE close; 
	TDS_CURSOR_STATE dealloc;
} TDS_CURSOR_STATUS;

typedef enum tds_cursor_operation
{
	TDS_CURSOR_POSITION = 0,
	TDS_CURSOR_UPDATE = 1,
	TDS_CURSOR_DELETE = 2,
	TDS_CURSOR_INSERT = 4
} TDS_CURSOR_OPERATION;

typedef enum tds_cursor_fetch
{
	TDS_CURSOR_FETCH_NEXT = 1,
	TDS_CURSOR_FETCH_PREV,
	TDS_CURSOR_FETCH_FIRST,
	TDS_CURSOR_FETCH_LAST,
	TDS_CURSOR_FETCH_ABSOLUTE,
	TDS_CURSOR_FETCH_RELATIVE
} TDS_CURSOR_FETCH;

/**
 * Holds informations about a cursor
 */
typedef struct tds_cursor
{
	struct tds_cursor *next;	/**< next in linked list, keep first */
	TDS_INT ref_count;		/**< reference counter so client can retain safely a pointer */
	TDS_TINYINT cursor_name_len;	/**< length of cursor name > 0 and <= 30  */
	char *cursor_name;		/**< name of the cursor */
	TDS_INT cursor_id;		/**< cursor id returned by the server after cursor declare */
	TDS_TINYINT options;		/**< read only|updatable */
	TDS_TINYINT hasargs;		/**< cursor parameters exists ? */
	TDS_USMALLINT query_len;	/**< SQL query length */
	char *query;                 	/**< SQL query */
	/* TODO for updatable columns */
	/* TDS_TINYINT number_upd_cols; */	/**< number of updatable columns */
	/* TDSUPDCOL *cur_col_list; */	/**< updatable column list */
	TDS_INT cursor_rows;		/**< number of cursor rows to fetch */
	/* TDSPARAMINFO *params; */	/** cursor parameter */
	TDS_CURSOR_STATUS status;
	TDS_SMALLINT srv_status;
	TDSRESULTINFO *res_info;	/** row fetched from this cursor */
	TDS_INT type, concurrency;
} TDSCURSOR;

/**
 * Current environment as reported by the server
 */
typedef struct tds_env
{
	int block_size;
	char *language;
	char *charset;
	char *database;
} TDSENV;

/**
 * Holds information for a dynamic (also called prepared) query.
 */
typedef struct tds_dynamic
{
	struct tds_dynamic *next;	/**< next in linked list, keep first */
	/** 
	 * id of dynamic.
	 * Usually this id correspond to server one but if not specified
	 * is generated automatically by libTDS
	 */
	char id[30];
	/* int dyn_state; */ /* TODO use it */
	/** numeric id for mssql7+*/
	TDS_INT num_id;
	TDSPARAMINFO *res_info;	/**< query results */
	/**
	 * query parameters.
	 * Mostly used executing query however is a good idea to prepare query
	 * again if parameter type change in an incompatible way (ie different
	 * types or larger size). Is also better to prepare a query knowing
	 * parameter types earlier.
	 */
	TDSPARAMINFO *params;
	/**
	 * this dynamic query cannot be prepared so libTDS have to construct a simple query.
	 * This can happen for instance is tds protocol doesn't support dynamics or trying
	 * to prepare query under Sybase that have BLOBs as parameters.
	 */
	int emulated;
	/** saved query, we need to know original query if prepare is impossible */
	char *query;
} TDSDYNAMIC;

typedef enum {
	TDS_MULTIPLE_QUERY,
	TDS_MULTIPLE_EXECUTE,
	TDS_MULTIPLE_RPC
} TDS_MULTIPLE_TYPE;

typedef struct tds_multiple
{
	TDS_MULTIPLE_TYPE type;
	unsigned int flags;
} TDSMULTIPLE;

/* forward declaration */
typedef struct tds_context TDSCONTEXT;
typedef int (*err_handler_t) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);

struct tds_context
{
	TDSLOCALE *locale;
	void *parent;
	/* handlers */
	int (*msg_handler) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
	int (*err_handler) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
	int (*int_handler) (void *);
};

enum TDS_ICONV_ENTRY
{ 
	  client2ucs2
	, client2server_chardata
	, iso2server_metadata
	, initial_char_conv_count	/* keep last */
};

typedef struct tds_authentication
{
	TDS_UCHAR *packet;
	int packet_len;
	int (*free)(TDSSOCKET * tds, struct tds_authentication * auth);
	int (*handle_next)(TDSSOCKET * tds, struct tds_authentication * auth, size_t len);
} TDSAUTHENTICATION;

/**
 * Information for a server connection
 */
struct tds_socket
{
	TDS_SYS_SOCKET s;		/**< tcp socket, INVALID_SOCKET if not connected */

	TDS_USMALLINT tds_version;
	TDS_UINT product_version;	/**< version of product (Sybase/MS and full version) */
	char *product_name;

	unsigned char capabilities[TDS_MAX_CAPABILITY];
	unsigned int broken_dates:1;
	unsigned int emul_little_endian:1;
	unsigned int use_iconv:1;
	unsigned int tds71rev1:1;
  
	unsigned char *in_buf;		/**< input buffer */
	unsigned char *out_buf;		/**< output buffer */
	unsigned int in_buf_max;	/**< allocated input buffer */
	unsigned in_pos;		/**< current position in in_buf */
	unsigned out_pos;		/**< current position in out_buf */
	unsigned in_len;		/**< input buffer length */

	unsigned char in_flag;		/**< input buffer type */
	unsigned char out_flag;		/**< output buffer type */
	void *parent;

	/**
	 * Current query information. 
	 * Contains information in process, both normal and compute results.
	 * This pointer shouldn't be freed; it's just an alias to another structure.
	 */
	TDSRESULTINFO *current_results;
	TDSRESULTINFO *res_info;
	TDS_INT num_comp_info;
	TDSCOMPUTEINFO **comp_info;
	TDSPARAMINFO *param_info;
	TDSCURSOR *cur_cursor;		/**< cursor in use */
	TDSCURSOR *cursors;		/**< linked list of cursors allocated for this connection */
	TDS_TINYINT has_status; 	/**< true is ret_status is valid */
	TDS_INT ret_status;     	/**< return status from store procedure */
	TDS_STATE state;

	volatile 
	unsigned char in_cancel; 	/**< indicate we are waiting a cancel reply; discard tokens till acknowledge */

	TDS_INT8 rows_affected;		/**< rows updated/deleted/inserted/selected, TDS_NO_COUNT if not valid */
	TDS_INT query_timeout;
	TDSENV env;

	TDSDYNAMIC *cur_dyn;		/**< dynamic structure in use */
	TDSDYNAMIC *dyns;		/**< list of dynamic allocate for this connection */

	const TDSCONTEXT *tds_ctx;
	int char_conv_count;
	TDSICONV **char_convs;

	TDSCONNECTION *connection;	/**< config for login stuff. After login this field is NULL */

	int spid;
	TDS_UCHAR collation[5];
	TDS_UCHAR tds9_transaction[8];
	void (*env_chg_func) (TDSSOCKET * tds, int type, char *oldval, char *newval);
	int internal_sp_called;

	void *tls_session;
	void *tls_credentials;
	TDSAUTHENTICATION *authentication;
	int option_value;
};

int tds_init_write_buf(TDSSOCKET * tds);
void tds_free_result_info(TDSRESULTINFO * info);
void tds_free_socket(TDSSOCKET * tds);
void tds_free_connection(TDSCONNECTION * connection);
void tds_free_all_results(TDSSOCKET * tds);
void tds_free_results(TDSRESULTINFO * res_info);
void tds_free_param_results(TDSPARAMINFO * param_info);
void tds_free_param_result(TDSPARAMINFO * param_info);
void tds_free_msg(TDSMESSAGE * message);
void tds_cursor_deallocated(TDSSOCKET *tds, TDSCURSOR *cursor);
void tds_release_cursor(TDSSOCKET *tds, TDSCURSOR *cursor);
void tds_free_bcp_column_data(BCPCOLDATA * coldata);

int tds_put_n(TDSSOCKET * tds, const void *buf, size_t n);
int tds_put_string(TDSSOCKET * tds, const char *buf, int len);
int tds_put_int(TDSSOCKET * tds, TDS_INT i);
int tds_put_int8(TDSSOCKET * tds, TDS_INT8 i);
int tds_put_smallint(TDSSOCKET * tds, TDS_SMALLINT si);
/** Output a tinyint value */
#define tds_put_tinyint(tds, ti) tds_put_byte(tds,ti)
int tds_put_byte(TDSSOCKET * tds, unsigned char c);
TDSRESULTINFO *tds_alloc_results(int num_cols);
TDSCOMPUTEINFO **tds_alloc_compute_results(TDSSOCKET * tds, int num_cols, int by_cols);
TDSCONTEXT *tds_alloc_context(void * parent);
void tds_free_context(TDSCONTEXT * locale);
TDSSOCKET *tds_alloc_socket(TDSCONTEXT * context, int bufsize);

/* config.c */
int tds_default_port(int major, int minor);
const TDS_COMPILETIME_SETTINGS *tds_get_compiletime_settings(void);
typedef void (*TDSCONFPARSE) (const char *option, const char *value, void *param);
int tds_read_conf_section(FILE * in, const char *section, TDSCONFPARSE tds_conf_parse, void *parse_param);
int tds_read_conf_file(TDSCONNECTION * connection, const char *server);
void tds_parse_conf_section(const char *option, const char *value, void *param);
TDSCONNECTION *tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale);
void tds_fix_connection(TDSCONNECTION * connection);
TDS_USMALLINT tds_config_verstr(const char *tdsver, TDSCONNECTION * connection);
int tds_lookup_host(const char *servername, char *ip);
int tds_set_interfaces_file_loc(const char *interfloc);
extern const char STD_DATETIME_FMT[];
int tds_config_boolean(const char *value);

TDSLOCALE *tds_get_locale(void);
int tds_alloc_row(TDSRESULTINFO * res_info);
int tds_alloc_compute_row(TDSCOMPUTEINFO * res_info);
BCPCOLDATA * tds_alloc_bcp_column_data(int column_size);
unsigned char *tds7_crypt_pass(const unsigned char *clear_pass, size_t len, unsigned char *crypt_pass);
TDSDYNAMIC *tds_lookup_dynamic(TDSSOCKET * tds, const char *id);
/*@observer@*/ const char *tds_prtype(int token);
int tds_get_varint_size(TDSSOCKET * tds, int datatype);
int tds_get_cardinal_type(int datatype, int usertype);



/* iconv.c */
void tds_iconv_open(TDSSOCKET * tds, const char *charset);
void tds_iconv_close(TDSSOCKET * tds);
void tds_srv_charset_changed(TDSSOCKET * tds, const char *charset);
void tds7_srv_charset_changed(TDSSOCKET * tds, int sql_collate, int lcid);
int tds_iconv_alloc(TDSSOCKET * tds);
void tds_iconv_free(TDSSOCKET * tds);
TDSICONV *tds_iconv_from_collate(TDSSOCKET * tds, TDS_UCHAR collate[5]);

/* threadsafe.c */
char *tds_timestamp_str(char *str, int maxlen);
struct tm *tds_localtime_r(const time_t *timep, struct tm *result);
struct hostent *tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct hostent *tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen,
				    int *h_errnop);
struct servent *tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen);
#ifdef INADDR_NONE
const char *tds_inet_ntoa_r(struct in_addr iaddr, char *ip, size_t len);
#endif
char *tds_get_homedir(void);

/* mem.c */
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO * old_param);
void tds_free_input_params(TDSDYNAMIC * dyn);
void tds_free_dynamic(TDSSOCKET * tds, TDSDYNAMIC * dyn);
TDSSOCKET *tds_realloc_socket(TDSSOCKET * tds, size_t bufsize);
char *tds_alloc_client_sqlstate(int msgno);
char *tds_alloc_lookup_sqlstate(TDSSOCKET * tds, int msgno);
TDSLOGIN *tds_alloc_login(void);
TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET * tds, const char *id);
void tds_free_login(TDSLOGIN * login);
TDSCONNECTION *tds_alloc_connection(TDSLOCALE * locale);
TDSLOCALE *tds_alloc_locale(void);
void *tds_alloc_param_data(TDSCOLUMN * curparam);
void tds_free_locale(TDSLOCALE * locale);
TDSCURSOR * tds_alloc_cursor(TDSSOCKET * tds, const char *name, TDS_INT namelen, const char *query, TDS_INT querylen);
void tds_free_row(TDSRESULTINFO * res_info, unsigned char *row);

/* login.c */
void tds_set_packet(TDSLOGIN * tds_login, int packet_size);
void tds_set_port(TDSLOGIN * tds_login, int port);
void tds_set_passwd(TDSLOGIN * tds_login, const char *password);
void tds_set_bulk(TDSLOGIN * tds_login, TDS_TINYINT enabled);
void tds_set_user(TDSLOGIN * tds_login, const char *username);
void tds_set_app(TDSLOGIN * tds_login, const char *application);
void tds_set_host(TDSLOGIN * tds_login, const char *hostname);
void tds_set_library(TDSLOGIN * tds_login, const char *library);
void tds_set_server(TDSLOGIN * tds_login, const char *server);
void tds_set_client_charset(TDSLOGIN * tds_login, const char *charset);
void tds_set_language(TDSLOGIN * tds_login, const char *language);
void tds_set_database_name(TDSLOGIN * tds_login, const char *dbname);
void tds_set_version(TDSLOGIN * tds_login, TDS_TINYINT major_ver, TDS_TINYINT minor_ver);
void tds_set_capabilities(TDSLOGIN * tds_login, unsigned char *capabilities, int size);
int tds_connect_and_login(TDSSOCKET * tds, TDSCONNECTION * connection);

/* query.c */
int tds_submit_query(TDSSOCKET * tds, const char *query);
int tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
int tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...);
int tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params);
int tds_submit_execdirect(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
int tds8_submit_prepexec(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params);
int tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn);
int tds_send_cancel(TDSSOCKET * tds);
const char *tds_next_placeholder(const char *start);
int tds_count_placeholders(const char *query);
int tds_needs_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn);
int tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn);
int tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params);
int tds_submit_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD command, TDS_OPTION option, TDS_OPTION_ARG *param, TDS_INT param_size);
int tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen);
int tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len);
const char *tds_skip_comment(const char *s);
const char *tds_skip_quoted(const char *s);

int tds_cursor_declare(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, int *send);
int tds_cursor_setrows(TDSSOCKET * tds, TDSCURSOR * cursor, int *send);
int tds_cursor_open(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, int *send);
int tds_cursor_fetch(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_FETCH fetch_type, TDS_INT i_row);
int tds_cursor_get_cursor_info(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_UINT * row_number, TDS_UINT * row_count);
int tds_cursor_close(TDSSOCKET * tds, TDSCURSOR * cursor);
int tds_cursor_dealloc(TDSSOCKET * tds, TDSCURSOR * cursor);
int tds_cursor_update(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_OPERATION op, TDS_INT i_row, TDSPARAMINFO * params);
int tds_cursor_setname(TDSSOCKET * tds, TDSCURSOR * cursor);

int tds_multiple_init(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDS_MULTIPLE_TYPE type);
int tds_multiple_done(TDSSOCKET *tds, TDSMULTIPLE *multiple);
int tds_multiple_query(TDSSOCKET *tds, TDSMULTIPLE *multiple, const char *query, TDSPARAMINFO * params);
int tds_multiple_execute(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDSDYNAMIC * dyn);

/* token.c */
int tds_process_cancel(TDSSOCKET * tds);
#ifdef WORDS_BIGENDIAN
void tds_swap_datatype(int coltype, unsigned char *buf);
#endif
void tds_swap_numeric(TDS_NUMERIC *num);
int tds_get_token_size(int marker);
int tds_process_login_tokens(TDSSOCKET * tds);
int tds_process_simple_query(TDSSOCKET * tds);
int tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG * tds_argument,
			TDS_INT * tds_argsize);
int tds_process_tokens(TDSSOCKET * tds, /*@out@*/ TDS_INT * result_type, /*@out@*/ int *done_flags, unsigned flag);

/* data.c */
void tds_set_param_type(TDSSOCKET * tds, TDSCOLUMN * curcol, TDS_SERVER_TYPE type);
void tds_set_column_type(TDSSOCKET * tds, TDSCOLUMN * curcol, int type);
TDS_INT tds_data_get_info(TDSSOCKET *tds, TDSCOLUMN *col);


/* tds_convert.c */
TDS_INT tds_datecrack(TDS_INT datetype, const void *di, TDSDATEREC * dr);
int tds_get_conversion_type(int srctype, int colsize);
extern const char tds_hex_digits[];

/* write.c */
int tds_flush_packet(TDSSOCKET * tds);
int tds_put_buf(TDSSOCKET * tds, const unsigned char *buf, int dsize, int ssize);

/* read.c */
unsigned char tds_get_byte(TDSSOCKET * tds);
void tds_unget_byte(TDSSOCKET * tds);
unsigned char tds_peek(TDSSOCKET * tds);
TDS_SMALLINT tds_get_smallint(TDSSOCKET * tds);
TDS_INT tds_get_int(TDSSOCKET * tds);
TDS_INT8 tds_get_int8(TDSSOCKET * tds);
int tds_get_string(TDSSOCKET * tds, int string_len, char *dest, size_t dest_size);
int tds_get_char_data(TDSSOCKET * tds, char *dest, size_t wire_size, TDSCOLUMN * curcol);
void *tds_get_n(TDSSOCKET * tds, /*@out@*/ /*@null@*/ void *dest, int n);
int tds_get_size_by_type(int servertype);


/* util.c */
int tdserror (const TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgno, int errnum);
TDS_STATE tds_set_state(TDSSOCKET * tds, TDS_STATE state);
void tds_set_parent(TDSSOCKET * tds, void *the_parent);
int tds_swap_bytes(unsigned char *buf, int bytes);
int tds_version(TDSSOCKET * tds_socket, char *pversion_string);
unsigned int tds_gettime_ms(void);
int tds_get_req_capability(TDSSOCKET * tds, int cap);

/* log.c */
void tdsdump_off(void);
void tdsdump_on(void);
int tdsdump_isopen(void);
#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility pop
#endif
int tdsdump_open(const char *filename);
#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility push(hidden)
#endif
void tdsdump_close(void);
void tdsdump_dump_buf(const char* file, unsigned int level_line, const char *msg, const void *buf, size_t length);
void tdsdump_col(const TDSCOLUMN *col);
#undef tdsdump_log
void tdsdump_log(const char* file, unsigned int level_line, const char *fmt, ...)
#if defined(__GNUC__) && __GNUC__ >= 2
	__attribute__ ((__format__ (__printf__, 3, 4)))
#endif
;
#define tdsdump_log if (TDS_UNLIKELY(tds_write_dump)) tdsdump_log

extern int tds_write_dump;
extern int tds_debug_flags;
extern int tds_g_append_mode;

/* net.c */
int tds_lastpacket(TDSSOCKET * tds);
TDSERRNO tds_open_socket(TDSSOCKET * tds, const char *ip_addr, unsigned int port, int timeout, int *p_oserr);
int tds_close_socket(TDSSOCKET * tds);
int tds_read_packet(TDSSOCKET * tds);
int tds_write_packet(TDSSOCKET * tds, unsigned char final);
int tds7_get_instance_ports(FILE *output, const char *ip_addr);
int tds7_get_instance_port(const char *ip_addr, const char *instance);
int tds_ssl_init(TDSSOCKET *tds);
void tds_ssl_deinit(TDSSOCKET *tds);
const char *tds_prwsaerror(int erc);



/* vstrbuild.c */
int tds_vstrbuild(char *buffer, int buflen, int *resultlen, char *text, int textlen, const char *formats, int formatlen,
		  va_list ap);

/* numeric.c */
char *tds_money_to_string(const TDS_MONEY * money, char *s);
TDS_INT tds_numeric_to_string(const TDS_NUMERIC * numeric, char *s);
TDS_INT tds_numeric_change_prec_scale(TDS_NUMERIC * numeric, unsigned char new_prec, unsigned char new_scale);

/* getmac.c */
void tds_getmac(TDS_SYS_SOCKET s, unsigned char mac[6]);

#ifndef HAVE_SSPI
TDSAUTHENTICATION * tds_ntlm_get_auth(TDSSOCKET * tds);
TDSAUTHENTICATION * tds_gss_get_auth(TDSSOCKET * tds);
#else
TDSAUTHENTICATION * tds_sspi_get_auth(TDSSOCKET * tds);
#endif

/* bulk.c */

/** bcp direction */
enum tds_bcp_directions
{
	TDS_BCP_IN = 1,
	TDS_BCP_OUT = 2,
	TDS_BCP_QUERYOUT = 3
};

typedef struct tds_bcpinfo
{
	const char *hint;
	void *parent;
	TDS_CHAR *tablename;
	TDS_CHAR *insert_stmt;
	TDS_INT direction;
	TDS_INT identity_insert_on;
	TDS_INT xfer_init;
	TDS_INT var_cols;
	TDS_INT bind_count;
	TDSRESULTINFO *bindinfo;
} TDSBCPINFO;

int tds_bcp_init(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
typedef int  (*tds_bcp_get_col_data) (TDSBCPINFO *bulk, TDSCOLUMN *bcpcol, int offset);
typedef void (*tds_bcp_null_error)   (TDSBCPINFO *bulk, int index, int offset);
int tds_bcp_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset);
int tds_bcp_done(TDSSOCKET *tds, int *rows_copied);
int tds_bcp_start(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
int tds_bcp_start_copy_in(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);

int tds_writetext_start(TDSSOCKET *tds, const char *objname, const char *textptr, const char *timestamp, int with_log, TDS_UINT size);
int tds_writetext_continue(TDSSOCKET *tds, const TDS_UCHAR *text, TDS_UINT size);
int tds_writetext_end(TDSSOCKET *tds);


#define IS_TDS42(x) (x->tds_version==0x402)
#define IS_TDS46(x) (x->tds_version==0x406)
#define IS_TDS50(x) (x->tds_version==0x500)
#define IS_TDS70(x) (x->tds_version==0x700)
#define IS_TDS71(x) (x->tds_version==0x701)
#define IS_TDS72(x) (x->tds_version==0x702)

#define IS_TDS7_PLUS(x) ((x)->tds_version>=0x700)
#define IS_TDS71_PLUS(x) ((x)->tds_version>=0x701)
#define IS_TDS72_PLUS(x) ((x)->tds_version>=0x702)

#define TDS_MAJOR(x) ((x)->tds_version >> 8)
#define TDS_MINOR(x) ((x)->tds_version & 0xff)

#define IS_TDSDEAD(x) (((x) == NULL) || TDS_IS_SOCKET_INVALID((x)->s))

/** Check if product is Sybase (such as Adaptive Server Enterrprice). x should be a TDS_SOCKET*. */
#define TDS_IS_SYBASE(x) (!(x->product_version & 0x80000000u))
/** Check if product is Microsft SQL Server. x should be a TDS_SOCKET*. */
#define TDS_IS_MSSQL(x) ((x->product_version & 0x80000000u)!=0)

/** Calc a version number for mssql. Use with TDS_MS_VER(7,0,842).
 * For test for a range of version you can use check like
 * if (tds->product_version >= TDS_MS_VER(7,0,0) && tds->product_version < TDS_MS_VER(8,0,0)) */
#define TDS_MS_VER(maj,min,x) (0x80000000u|((maj)<<24)|((min)<<16)|(x))

/* TODO test if not similar to ms one*/
/** Calc a version number for Sybase. */
#define TDS_SYB_VER(maj,min,x) (((maj)<<24)|((min)<<16)|(x)<<8)

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#pragma GCC visibility pop
#endif

#define TDS_PUT_INT(tds,v) tds_put_int((tds), ((TDS_INT)(v)))
#define TDS_PUT_SMALLINT(tds,v) tds_put_smallint((tds), ((TDS_SMALLINT)(v)))
#define TDS_PUT_BYTE(tds,v) tds_put_byte((tds), ((unsigned char)(v)))

#endif /* _tds_h_ */
