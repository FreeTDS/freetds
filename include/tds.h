/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

static char rcsid_tds_h[] = "$Id: tds.h,v 1.184 2004-07-26 14:39:40 freddy77 Exp $";
static void *no_unused_tds_h_warn[] = { rcsid_tds_h, no_unused_tds_h_warn };

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* forward declaration */
typedef struct tdsiconvinfo TDSICONV;
typedef struct tds_socket TDSSOCKET;

#include "tdsver.h"
#include "tds_sysdep_public.h"
#ifdef _FREETDS_LIBRARY_SOURCE
#include "tds_sysdep_private.h"
#endif /* _FREETDS_LIBRARY_SOURCE */

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

typedef struct _tds_compiletime_settings
{
	const char *freetds_version;	/* release version of FreeTDS */
	const char *last_update;	/* latest software_version date among the modules */
	int msdblib;		/* for MS style dblib */
	int sybase_compat;	/* enable increased Open Client binary compatibility */
	int threadsafe;		/* compile for thread safety default=no */
	int libiconv;		/* search for libiconv in DIR/include and DIR/lib */
	const char *tdsver;	/* TDS protocol version (4.2/4.6/5.0/7.0/8.0) 5.0 */
	int iodbc;		/* build odbc driver against iODBC in DIR */
	int unixodbc;		/* build odbc driver against unixODBC in DIR */

} TDS_COMPILETIME_SETTINGS;

struct DSTR_STRUCT {
	/* keep always at last */
	char dstr_s[1];
};
typedef struct DSTR_STRUCT *DSTR;

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
	TDS_INT len;
	TDS_CHAR array[256];
} TDS_VARBINARY;
typedef struct tdsvarchar
{
	TDS_INT len;
	TDS_CHAR array[256];
} TDS_VARCHAR;

typedef struct tdsunique
{
	TDS_UINT Data1;
	TDS_USMALLINT Data2;
	TDS_USMALLINT Data3;
	TDS_UCHAR Data4[8];
} TDS_UNIQUE;

/** information on data, used by tds_datecrack */
typedef struct tdsdaterec
{
	TDS_INT year;	       /**< year */
	TDS_INT month;	       /**< month number (0-11) */
	TDS_INT day;	       /**< day of month (1-31) */
	TDS_INT dayofyear;     /**< day of year  (1-366) */
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

#define TDS_REG_ROW          -1
#define TDS_NO_MORE_ROWS     -2
#define TDS_COMP_ROW         -3
#define TDS_END_ROW          -4

#define TDS_INT_EXIT 0
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

enum tds_end
{
	  TDS_DONE_FINAL 	= 0x00	/* final result set, command completed successfully. */
	, TDS_DONE_MORE_RESULTS = 0x01	/* more results follow */
	, TDS_DONE_ERROR 	= 0x02	/* error occurred */
	, TDS_DONE_INXACT 	= 0x04	/* transaction in progress */
	, TDS_DONE_PROC 	= 0x08	/* results are from a stored procedure */
	, TDS_DONE_COUNT 	= 0x10	/* count field in packet is valid */
	, TDS_DONE_CANCELLED 	= 0x20	/* acknowledging an attention command (usually a cancel) */
	, TDS_DONE_EVENT 	= 0x40	/* part of an event notification. */
	, TDS_DONE_SRVERROR 	= 0x100	/* SQL server server error */
	
	/* after the above flags, a TDS_DONE packet has a field describing the state of the transaction */
	, TDS_DONE_NO_TRAN 	= 0	/* No transaction in effect */
	, TDS_DONE_TRAN_SUCCEED = 1	/* Transaction completed successfully */
	, TDS_DONE_TRAN_PROGRESS= 2	/* Transaction in progress */
	, TDS_DONE_STMT_ABORT 	= 3	/* A statement aborted */
	, TDS_DONE_TRAN_ABORT 	= 4	/* Transaction aborted */
};

/*
 * TDS_ERROR indicates a successful processing, but that a TDS_ERROR_TOKEN or TDS_EED_TOKEN error was encountered.  
 * TDS_FAIL indicates an unrecoverable failure.
 */
#define TDS_ERROR            3
#define TDS_DONT_RETURN      42

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
#define TDS_CMP_ROW_TOKEN         211	/* 0xD3                              */
#define TDS5_PARAMS_TOKEN         215	/* 0xD7    TDS 5.0 only              */
#define TDS_CAPABILITY_TOKEN      226	/* 0xE2                              */
#define TDS_ENVCHANGE_TOKEN       227	/* 0xE3                              */
#define TDS_EED_TOKEN             229	/* 0xE5                              */
#define TDS_DBRPC_TOKEN           230	/* 0xE6                              */
#define TDS5_DYNAMIC_TOKEN        231	/* 0xE7    TDS 5.0 only              */
#define TDS5_PARAMFMT_TOKEN       236	/* 0xEC    TDS 5.0 only              */
#define TDS_AUTH_TOKEN            237	/* 0xED                              */
#define TDS_RESULT_TOKEN          238	/* 0xEE                              */
#define TDS_DONE_TOKEN            253	/* 0xFD    TDS_DONE                  */
#define TDS_DONEPROC_TOKEN        254	/* 0xFE    TDS_DONEPROC              */
#define TDS_DONEINPROC_TOKEN      255	/* 0xFF    TDS_DONEINPROC            */

/* CURSOR support: TDS 5.0 only*/
#define TDS_CURCLOSE_TOKEN        128  /* 0x80    TDS 5.0 only              */
#define TDS_CURFETCH_TOKEN        130  /* 0x82    TDS 5.0 only              */
#define TDS_CURINFO_TOKEN         131  /* 0x83    TDS 5.0 only              */
#define TDS_CUROPEN_TOKEN         132  /* 0x84    TDS 5.0 only              */
#define TDS_CURDECLARE_TOKEN      134  /* 0x86    TDS 5.0 only              */

/* 
 * Cursor Declare, SetRows, Open and Close all return 0x83 token. 
 * But only SetRows includes the rowcount (4 byte) in the stream. 
 * So for Setrows we read the rowcount from the stream and not for others. 
 * These values are useful to determine when to read the rowcount from the packet
 */
#define IS_DECLARE  100
#define IS_CURROW   200
#define IS_OPEN     300
#define IS_CLOSE    400

/* states for tds_process_messages() */
#define PROCESS_ROWS    0
#define PROCESS_RESULTS 1
#define CANCEL_PROCESS  2
#define GOTO_1ST_ROW    3
#define LOGIN           4

/* environment type field */
#define TDS_ENV_DATABASE  	1
#define TDS_ENV_LANG      	2
#define TDS_ENV_CHARSET   	3
#define TDS_ENV_PACKSIZE  	4
#define TDS_ENV_LCID        	5
#define TDS_ENV_SQLCOLLATION	7

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
	SYBINT8 = 127,		/* 0x7F */
#define SYBINT8	SYBINT8
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
	SYBNVARCHAR = 103,	/* 0x67 */
#define SYBNVARCHAR	SYBNVARCHAR
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
	SYBLONGBINARY = 225,	/* 0xE1 */
#define SYBLONGBINARY	SYBLONGBINARY
	SYBSINT1 = 64,		/* 0x40 */
#define SYBSINT1	SYBSINT1
	SYBUINT2 = 65,		/* 0x41 */
#define SYBUINT2	SYBUINT2
	SYBUINT4 = 66,		/* 0x42 */
#define SYBUINT4	SYBUINT4
	SYBUINT8 = 67,		/* 0x43 */
#define SYBUINT8	SYBUINT8

	SYBUNIQUE = 36,		/* 0x24 */
#define SYBUNIQUE	SYBUNIQUE
	SYBVARIANT = 98 	/* 0x62 */
#define SYBVARIANT	SYBVARIANT
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

#define TDS_ZERO_FREE(x) {free((x)); (x) = NULL;}
#define TDS_VECTOR_SIZE(x) (sizeof(x)/sizeof(x[0]))

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

/* FIX ME -- not a complete list */
#define is_fixed_type(x) (x==SYBINT1    || \
			x==SYBINT2      || \
			x==SYBINT4      || \
			x==SYBINT8      || \
			x==SYBREAL	 || \
			x==SYBFLT8      || \
			x==SYBDATETIME  || \
			x==SYBDATETIME4 || \
			x==SYBBIT       || \
			x==SYBMONEY     || \
			x==SYBMONEY4    || \
			x==SYBUNIQUE)
#define is_nullable_type(x) ( \
			x==SYBBITN      || \
                     x==SYBINTN      || \
                     x==SYBFLTN      || \
                     x==SYBMONEYN    || \
                     x==SYBDATETIMN  || \
                     x==SYBVARCHAR   || \
                     x==SYBBINARY    || \
                     x==SYBVARBINARY || \
                     x==SYBTEXT      || \
                     x==SYBNTEXT     || \
                     x==SYBIMAGE)

#define is_blob_type(x) (x==SYBTEXT || x==SYBIMAGE || x==SYBNTEXT)
/* large type means it has a two byte size field */
/* define is_large_type(x) (x>128) */
#define is_numeric_type(x) (x==SYBNUMERIC || x==SYBDECIMAL)
#define is_unicode_type(x) (x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_collate_type(x) (x==XSYBVARCHAR || x==XSYBCHAR || x==SYBTEXT || x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_ascii_type(x) ( x==XSYBCHAR || x==XSYBVARCHAR || x==SYBTEXT || x==SYBCHAR || x==SYBVARCHAR)
#define is_binary_type(x) (x==SYBLONGBINARY)
#define is_char_type(x) (is_unicode_type(x) || is_ascii_type(x))
#define is_similar_type(x, y) ((is_char_type(x) && is_char_type(y)) || ((is_unicode_type(x) && is_unicode_type(y))))


#define TDS_MAX_CAPABILITY	22
#define MAXPRECISION 		80
#define TDS_MAX_CONN		4096
#define TDS_MAX_DYNID_LEN	30

/* defaults to use if no others are found */
#define TDS_DEF_SERVER		"SYBASE"
#define TDS_DEF_BLKSZ		512
#define TDS_DEF_CHARSET		"iso_1"
#define TDS_DEF_LANG		"us_english"
#if TDS42
#define TDS_DEF_MAJOR		4
#define TDS_DEF_MINOR		2
#define TDS_DEF_PORT		1433
#elif TDS46
#define TDS_DEF_MAJOR		4
#define TDS_DEF_MINOR		6
#define TDS_DEF_PORT		4000
#elif TDS70
#define TDS_DEF_MAJOR		7
#define TDS_DEF_MINOR		0
#define TDS_DEF_PORT		1433
#elif TDS80
#define TDS_DEF_MAJOR		8
#define TDS_DEF_MINOR		0
#define TDS_DEF_PORT		1433
#else
#define TDS_DEF_MAJOR		5
#define TDS_DEF_MINOR		0
#define TDS_DEF_PORT		4000
#endif

/* normalized strings from freetds.conf file */
#define TDS_STR_VERSION  "tds version"
#define TDS_STR_BLKSZ    "initial block size"
#define TDS_STR_SWAPDT   "swap broken dates"
#define TDS_STR_SWAPMNY  "swap broken money"
#define TDS_STR_TRYSVR   "try server login"
#define TDS_STR_TRYDOM   "try domain login"
#define TDS_STR_DOMAIN   "nt domain"
#define TDS_STR_XDOMAUTH "cross domain login"
#define TDS_STR_DUMPFILE "dump file"
#define TDS_STR_DEBUGLVL "debug level"
#define TDS_STR_TIMEOUT  "timeout"
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

/* TODO do a better check for alignment than this */
typedef union
{
	void *p;
	int i;
} tds_align_struct;

#define TDS_ALIGN_SIZE sizeof(tds_align_struct)

#define TDS_MAX_LOGIN_STR_SZ 30
typedef struct tds_login
{
	DSTR server_name;
	int port;
	TDS_TINYINT major_version;	/* TDS version */
	TDS_TINYINT minor_version;	/* TDS version */
	int block_size;
	DSTR language;		/* ie us-english */
	DSTR server_charset;	/*  ie iso_1 */
	TDS_INT connect_timeout;
	DSTR host_name;
	DSTR app_name;
	DSTR user_name;
	DSTR password;
	
	DSTR library;	/* Ct-Library, DB-Library,  TDS-Library or ODBC */
	TDS_TINYINT bulk_copy;
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted;

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func) (void *param);
	void *longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	DSTR client_charset;
} TDSLOGIN;

typedef struct tds_connection
{
	/* first part of structure is the same of login one */
	DSTR server_name; /**< server name (in freetds.conf) */
	int port;	   /**< port of database service */
	TDS_TINYINT major_version;
	TDS_TINYINT minor_version;
	int block_size;
	DSTR language;
	DSTR server_charset;	/**< charset of server */
	TDS_INT connect_timeout;
	DSTR host_name;	    /**< client hostname */
	DSTR app_name;
	DSTR user_name;	    /**< account for login */
	DSTR password;	    /**< password of account login */
	DSTR library;
	TDS_TINYINT bulk_copy;
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted;

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func) (void *param);
	void *longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	DSTR client_charset;

	DSTR ip_addr;	  /**< ip of server */
	DSTR database;
	DSTR dump_file;
	DSTR default_domain;
	int timeout;
	int debug_level;
	int text_size;
	int broken_dates;
	int broken_money;
	int try_server_login;
	int try_domain_login;
	int xdomain_auth;
	int emul_little_endian;
} TDSCONNECTION;

typedef struct tds_locale
{
	char *language;
	char *char_set;
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
 * TDS 8.0 collation information.
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
typedef struct _tds_encoding
{
	const char *name;
	unsigned char min_bytes_per_char;
	unsigned char max_bytes_per_char;
} TDS_ENCODING;

typedef struct _tds_bcpcoldata
{
	TDS_UCHAR *data;
	TDS_INT    datalen;
	TDS_INT    null_column;
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

	TDS_TINYINT column_namelen;	/**< length of column name */
	TDS_TINYINT table_namelen;
	struct
	{
		TDS_SMALLINT column_type;	/**< type of data, saved from wire */
		TDS_INT column_size;
	} on_server;

	const TDSICONV *char_conv;	/**< refers to previously allocated iconv information */

	TDS_CHAR table_name[TDS_SYSNAME_SIZE];
	TDS_CHAR column_name[TDS_SYSNAME_SIZE];

	TDS_INT column_offset;		/**< offset into row buffer for store data */
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
	/** size written in variable (ie: char, text, binary) */
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
	BCPCOLDATA *bcp_column_data;
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
	int null_info_size;
	unsigned char *current_row;

	TDS_SMALLINT rows_exist;
	TDS_INT row_count;
	TDS_SMALLINT computeid;
	TDS_TINYINT more_results;
	TDS_TINYINT *bycolumns;
	TDS_SMALLINT by_cols;
} TDSRESULTINFO;

/* values for tds->state */
enum
{
	TDS_QUERYING,
	TDS_PENDING,
	TDS_IDLE,
	TDS_CANCELED,
	TDS_DEAD
};

#define TDS_DBG_FUNC    7
#define TDS_DBG_INFO2   6
#define TDS_DBG_INFO1   5
#define TDS_DBG_NETWORK 4
#define TDS_DBG_WARN    3
#define TDS_DBG_ERROR   2
#define TDS_DBG_SEVERE  1

/**
 * An attempt at better logging.
 * Using these bitmapped values, various logging features can be turned on and off.
 * It can be especially helpful to turn packet data on/off for security reasons.
 */
enum TDS_DBG_LOG_STATE
{
	  TDS_DBG_LOGIN = 1		/* for diagnosing login problems;                                       
				 	   otherwise the username/password information is suppressed. */
	, TDS_DBG_API =    (1 << 1)	/* Log calls to client libraries */
	, TDS_DBG_ASYNC =  (1 << 2)	/* Log asynchronous function starts or completes. */
	, TDS_DBG_DIAG =   (1 << 3)	/* Log client- and server-generated messages */
	, TDS_DBG_error =  (1 << 4)
	/* TODO:  ^^^^^ make upper case when old #defines (above) are removed */
	/* Log FreeTDS runtime/logic error occurs. */
	, TDS_DBG_PACKET = (1 << 5)	/* Log hex dump of packets to/from the server. */
	, TDS_DBG_LIBTDS = (1 << 6)	/* Log calls to (and in) libtds */
	, TDS_DBG_CONFIG = (1 << 7)	/* replaces TDSDUMPCONFIG */
	, TDS_DBG_DEFAULT = 0xFE	/* all above except login packets */
};

typedef struct tds_result_info TDSCOMPUTEINFO;

typedef TDSRESULTINFO TDSPARAMINFO;

typedef struct tds_message
{
	TDS_SMALLINT priv_msg_type;
	TDS_SMALLINT line_number;
	TDS_UINT msg_number;
	TDS_SMALLINT msg_state;
	TDS_SMALLINT msg_level;
	TDS_CHAR *server;
	TDS_CHAR *message;
	TDS_CHAR *proc_name;
	TDS_CHAR *sql_state;
} TDSMESSAGE;

typedef struct tds_upd_col
{
	struct tds_upd_col *next;	
	TDS_INT colnamelength;
	char * columnname;
} TDSUPDCOL;

typedef enum {
	  TDS_CURSOR_STATE_UNACTIONED = 0
	, TDS_CURSOR_STATE_REQUESTED = 1	/* called by ct_cursor */ 
	, TDS_CURSOR_STATE_SENT = 2		/* sent to server and ack */
} TDS_CURSOR_STATE;

typedef struct _tds_cursor_status
{
	TDS_CURSOR_STATE declare;
	TDS_CURSOR_STATE cursor_row;
	TDS_CURSOR_STATE open;
	TDS_CURSOR_STATE fetch;
	TDS_CURSOR_STATE close; 
	TDS_CURSOR_STATE dealloc;
} TDS_CURSOR_STATUS;

typedef struct _tds_cursor 
{
	TDS_INT length;			/* total length of the remaining datastream */
	TDS_TINYINT cursor_name_len;	/* length of cursor name > 0 and <= 30  */
	char *cursor_name;		/* name of the cursor */
	TDS_INT cursor_id;		/* cursor id returned by the server after cursor declare */
	TDS_TINYINT options;		/* read only|updatable */
	TDS_TINYINT hasargs;		/* cursor parameters exists ? */
	TDS_USMALLINT query_len;	/* SQL query length */
	char *query;                 	/* SQL query */
	/* TODO for updatable columns */
	TDS_TINYINT number_upd_cols;	/* number of updatable columns */
	TDS_INT cursor_rows;		/* number of cursor rows to fetch */
	/*TODO when cursor has parameters*/
	/*TDS_PARAM *param_list;	 cursor parameter */
	TDSUPDCOL *cur_col_list;	/* updatable column list */
	TDS_CURSOR_STATUS status;
} TDS_CURSOR;

/*
 * Current environment as reported by the server
 */
typedef struct tds_env
{
	int block_size;
	char *language;
	char *charset;
	char *database;
} TDSENV;

typedef struct tds_dynamic
{
	char id[30];
	int dyn_state;
	/** numeric id for mssql7+*/
	TDS_INT num_id;
	TDSPARAMINFO *res_info;
	TDSPARAMINFO *params;
	int emulated;
	/** saved query, we need to know original query if prepare is impossible*/
	char *query;
} TDSDYNAMIC;

/* forward declaration */
typedef struct tds_context TDSCONTEXT;

struct tds_context
{
	TDSLOCALE *locale;
	void *parent;
	/* handler */
	int (*msg_handler) (TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
	int (*err_handler) (TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
};

enum TDS_ICONV_ENTRY
{ 
	  client2ucs2
	, client2server_chardata
	, iso2server_metadata
	, initial_char_conv_count	/* keep last */
};

struct tds_socket
{
	/* fixed and connect time */
	TDS_SYS_SOCKET s;
	TDS_SMALLINT major_version;
	TDS_SMALLINT minor_version;
	/** version of product (Sybase/MS and full version) */
	TDS_UINT product_version;
	char *product_name;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	unsigned char broken_dates;
	unsigned char option_flag2;
	/* in/out buffers */
	unsigned char *in_buf;
	unsigned char *out_buf;
	unsigned int in_buf_max;
	unsigned in_pos;
	unsigned out_pos;
	unsigned in_len;
	unsigned out_len;
	unsigned char in_flag;
	unsigned char out_flag;
	unsigned char last_packet;
	void *parent;
	/* info about current query. 
	 * Contain information in process, even normal results and compute.
	 * This pointer shouldn't be freed.
	 */
	TDSRESULTINFO *curr_resinfo;
	TDSRESULTINFO *res_info;
	TDS_INT num_comp_info;
	TDSCOMPUTEINFO **comp_info;
	TDSPARAMINFO *param_info;
	TDS_CURSOR *cursor;
	TDS_TINYINT has_status;
	TDS_INT ret_status;
	TDS_TINYINT state;
	int rows_affected;
	/* timeout stuff from Jeff */
	TDS_INT timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func) (void *param);
	void *longquery_param;
	time_t queryStarttime;
	TDSENV *env;
	/* dynamic placeholder stuff */
	int num_dyns;
	TDSDYNAMIC *cur_dyn;
	TDSDYNAMIC **dyns;
	int emul_little_endian;
	char *date_fmt;
	TDSCONTEXT *tds_ctx;
	int char_conv_count;
	TDSICONV **char_convs;

	/** config for login stuff. After login this field is NULL */
	TDSCONNECTION *connection;
	int spid;
	TDS_UCHAR collation[5];
	void (*env_chg_func) (TDSSOCKET * tds, int type, char *oldval, char *newval);
	int (*chkintr) (TDSSOCKET * tds);
	int (*hndlintr) (TDSSOCKET * tds);
    int internal_sp_called;
};

void tds_set_longquery_handler(TDSLOGIN * tds_login, void (*longquery_func) (void *param), void *longquery_param);
int tds_init_write_buf(TDSSOCKET * tds);
void tds_free_result_info(TDSRESULTINFO * info);
void tds_free_socket(TDSSOCKET * tds);
void tds_free_connection(TDSCONNECTION * connection);
void tds_free_all_results(TDSSOCKET * tds);
void tds_free_results(TDSRESULTINFO * res_info);
void tds_free_param_results(TDSPARAMINFO * param_info);
void tds_free_msg(TDSMESSAGE * message);
void tds_free_cursor(TDS_CURSOR *cursor);
void tds_free_bcp_column_data(BCPCOLDATA * coldata);

int tds_put_n(TDSSOCKET * tds, const void *buf, int n);
int tds_put_string(TDSSOCKET * tds, const char *buf, int len);
int tds_put_int(TDSSOCKET * tds, TDS_INT i);
int tds_put_int8(TDSSOCKET * tds, TDS_INT8 i);
int tds_put_smallint(TDSSOCKET * tds, TDS_SMALLINT si);
/** Output a tinyint value */
#define tds_put_tinyint(tds, ti) tds_put_byte(tds,ti)
int tds_put_byte(TDSSOCKET * tds, unsigned char c);
TDSRESULTINFO *tds_alloc_results(int num_cols);
TDSCOMPUTEINFO **tds_alloc_compute_results(TDS_INT * num_comp_results, TDSCOMPUTEINFO ** ci, int num_cols, int by_cols);
TDSCONTEXT *tds_alloc_context(void);
void tds_free_context(TDSCONTEXT * locale);
TDSSOCKET *tds_alloc_socket(TDSCONTEXT * context, int bufsize);

/* config.c */
const TDS_COMPILETIME_SETTINGS *tds_get_compiletime_settings(void);
typedef void (*TDSCONFPARSE) (const char *option, const char *value, void *param);
int tds_read_conf_section(FILE * in, const char *section, TDSCONFPARSE tds_conf_parse, void *parse_param);
int tds_read_conf_file(TDSCONNECTION * connection, const char *server);
TDSCONNECTION *tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale);
void tds_fix_connection(TDSCONNECTION * connection);
void tds_config_verstr(const char *tdsver, TDSCONNECTION * connection);
void tds_lookup_host(const char *servername, char *ip);
int tds_set_interfaces_file_loc(const char *interfloc);

TDSLOCALE *tds_get_locale(void);
unsigned char *tds_alloc_row(TDSRESULTINFO * res_info);
unsigned char *tds_alloc_compute_row(TDSCOMPUTEINFO * res_info);
BCPCOLDATA * tds_alloc_bcp_column_data(int column_size);
int tds_alloc_get_string(TDSSOCKET * tds, char **string, int len);
void tds_set_null(unsigned char *current_row, int column);
void tds_clr_null(unsigned char *current_row, int column);
int tds_get_null(unsigned char *current_row, int column);
unsigned char *tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass);
TDSDYNAMIC *tds_lookup_dynamic(TDSSOCKET * tds, char *id);
const char *tds_prtype(int token);



/* iconv.c */
void tds_iconv_open(TDSSOCKET * tds, const char *charset);
void tds_iconv_close(TDSSOCKET * tds);
void tds_srv_charset_changed(TDSSOCKET * tds, const char *charset);
void tds7_srv_charset_changed(TDSSOCKET * tds, int lcid);
int tds_iconv_alloc(TDSSOCKET * tds);
void tds_iconv_free(TDSSOCKET * tds);
TDSICONV *tds_iconv_from_lcid(TDSSOCKET * tds, int lcid);

/* threadsafe.c */
char *tds_timestamp_str(char *str, int maxlen);
struct hostent *tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct hostent *tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen,
				    int *h_errnop);
struct servent *tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen);
char *tds_get_homedir(void);

/* mem.c */
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO * old_param);
void tds_free_input_params(TDSDYNAMIC * dyn);
void tds_free_all_dynamic(TDSSOCKET * tds);
void tds_free_dynamic(TDSSOCKET * tds, TDSDYNAMIC * dyn);
TDSSOCKET *tds_realloc_socket(TDSSOCKET * tds, int bufsize);
void tds_free_compute_result(TDSCOMPUTEINFO * comp_info);
void tds_free_compute_results(TDSCOMPUTEINFO ** comp_info, TDS_INT num_comp);
unsigned char *tds_alloc_param_row(TDSPARAMINFO * info, TDSCOLUMN * curparam);
char *tds_alloc_client_sqlstate(int msgnum);
char *tds_alloc_lookup_sqlstate(TDSSOCKET * tds, int msgnum);
TDSLOGIN *tds_alloc_login(void);
TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET * tds, const char *id);
void tds_free_login(TDSLOGIN * login);
TDSCONNECTION *tds_alloc_connection(TDSLOCALE * locale);
TDSLOCALE *tds_alloc_locale(void);
void tds_free_locale(TDSLOCALE * locale);
TDS_CURSOR * tds_alloc_cursor(char *name, TDS_INT namelen, char *query, TDS_INT querylen);

/* login.c */
int tds7_send_auth(TDSSOCKET * tds, const unsigned char *challenge);
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
void tds_set_version(TDSLOGIN * tds_login, short major_ver, short minor_ver);
void tds_set_capabilities(TDSLOGIN * tds_login, unsigned char *capabilities, int size);
int tds_connect(TDSSOCKET * tds, TDSCONNECTION * connection);

/* query.c */
int tds_submit_query(TDSSOCKET * tds, const char *query);
int tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
int tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...);
int tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params);
int tds_submit_execdirect(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
int tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn);
int tds_send_cancel(TDSSOCKET * tds);
const char *tds_next_placeholders(const char *start);
int tds_count_placeholders(const char *query);
int tds_get_dynid(TDSSOCKET * tds, char **id);
int tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn);
int tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params);
int tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen);
int tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len);
const char *tds_skip_quoted(const char *s);
int tds_cursor_declare(TDSSOCKET * tds, int *send);
int tds_cursor_setrows(TDSSOCKET * tds, int *send);
int tds_cursor_open(TDSSOCKET * tds, int *send);
int tds_cursor_fetch(TDSSOCKET * tds);
int tds_cursor_close(TDSSOCKET * tds);
int tds_cursor_dealloc(TDSSOCKET * tds);

/* token.c */
int tds_process_cancel(TDSSOCKET * tds);
void tds_swap_datatype(int coltype, unsigned char *buf);
int tds_get_token_size(int marker);
int tds_process_login_tokens(TDSSOCKET * tds);
void tds_add_row_column_size(TDSRESULTINFO * info, TDSCOLUMN * curcol);
int tds_process_simple_query(TDSSOCKET * tds);
int tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG * tds_argument,
			TDS_INT * tds_argsize);
int tds_process_result_tokens(TDSSOCKET * tds, TDS_INT * result_type, int *done_flags);
int tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid);
int tds_process_row_tokens_ct(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT *computeid);
int tds_process_trailing_tokens(TDSSOCKET * tds);
int tds_client_msg(TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgnum, int level, int state, int line, const char *message);
int tds_do_until_done(TDSSOCKET * tds);

/* data.c */
void tds_set_param_type(TDSSOCKET * tds, TDSCOLUMN * curcol, TDS_SERVER_TYPE type);
void tds_set_column_type(TDSCOLUMN * curcol, int type);


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
int tds_get_string(TDSSOCKET * tds, int string_len, char *dest, size_t dest_size);
int tds_get_char_data(TDSSOCKET * tds, char *dest, size_t wire_size, TDSCOLUMN * curcol);
void *tds_get_n(TDSSOCKET * tds, void *dest, int n);
int tds_get_size_by_type(int servertype);
int tds_read_packet(TDSSOCKET * tds);

/* util.c */
void tds_set_parent(TDSSOCKET * tds, void *the_parent);
void *tds_get_parent(TDSSOCKET * tds);
void tds_ctx_set_parent(TDSCONTEXT * ctx, void *the_parent);
void *tds_ctx_get_parent(TDSCONTEXT * ctx);
int tds_swap_bytes(unsigned char *buf, int bytes);
int tds_version(TDSSOCKET * tds_socket, char *pversion_string);
void tdsdump_off(void);
void tdsdump_on(void);
int tdsdump_open(const char *filename);
int tdsdump_append(void);
void tdsdump_close(void);
void tdsdump_dump_buf(const void *buf, int length);
void tdsdump_log(int dbg_lvl, const char *fmt, ...);
int tds_close_socket(TDSSOCKET * tds);

/* vstrbuild.c */
int tds_vstrbuild(char *buffer, int buflen, int *resultlen, char *text, int textlen, const char *formats, int formatlen,
		  va_list ap);

/* numeric.c */
char *tds_money_to_string(const TDS_MONEY * money, char *s);
TDS_INT tds_numeric_to_string(const TDS_NUMERIC * numeric, char *s);

/* getmac.c */
void tds_getmac(int s, unsigned char mac[6]);

typedef struct tds_answer
{
	unsigned char lm_resp[24];
	unsigned char nt_resp[24];
} TDSANSWER;
void tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDSANSWER * answer);

#define IS_TDS42(x) (x->major_version==4 && x->minor_version==2)
#define IS_TDS46(x) (x->major_version==4 && x->minor_version==6)
#define IS_TDS50(x) (x->major_version==5 && x->minor_version==0)
#define IS_TDS70(x) (x->major_version==7 && x->minor_version==0)
#define IS_TDS80(x) (x->major_version==8 && x->minor_version==0)

#define IS_TDS7_PLUS(x) ( IS_TDS70(x) || IS_TDS80(x) )

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

#endif /* _tds_h_ */
