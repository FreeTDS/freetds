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

#ifndef _tds_h_
#define _tds_h_

static char rcsid_tds_h[]=
	"$Id: tds.h,v 1.128 2003-05-28 19:29:53 freddy77 Exp $";
static void *no_unused_tds_h_warn[] = {
	rcsid_tds_h,
	no_unused_tds_h_warn};

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#if HAVE_ICONV
#include <iconv.h>
#endif

#include "tdsver.h"
#include "tds_sysdep_public.h"
#ifdef _FREETDS_LIBRARY_SOURCE
#include "tds_sysdep_private.h"
#endif /* _FREETDS_LIBRARY_SOURCE */

#ifdef __cplusplus
extern "C" {
#endif 

/**
 * A structure to hold all the compile-time settings.
 * This structure is returned by tds_get_compiletime_settings
 */
 
typedef struct _tds_compiletime_settings
{
	const char *freetds_version;		/* release version of FreeTDS */
	const char *last_update;	/* latest software_version date among the modules */
	int msdblib;		/* for MS style dblib */
	int sybase_compat;	/* enable increased Open Client binary compatibility */
	int threadsafe; 	/* compile for thread safety default=no */
	int libiconv;     	/* search for libiconv in DIR/include and DIR/lib */
	const char *tdsver;	/* TDS protocol version (4.2/4.6/5.0/7.0/8.0) 5.0 */
	int iodbc;		/* build odbc driver against iODBC in DIR */
	int unixodbc; 		/* build odbc driver against unixODBC in DIR */

} TDS_COMPILETIME_SETTINGS;

struct DSTR_CHAR;
typedef struct DSTR_CHAR *DSTR;

/**
 * @file tds.h
 * Main include file for libtds
 */

/**
 * \defgroup libtds LibTDS API
 * Callable functions in \c libtds.
 * 
   The \c libtds library is for use internal to \em FreeTDS.  It is not
   intended for use by applications.  Although any use is \em permitted, you're
   encouraged to use one of the established public APIs instead, because their
   interfaces are stable and documented by the vendors.  
 */

/* 
** I've tried to change all references to data that goes to 
** or comes off the wire to use these typedefs.  I've probably 
** missed a bunch, but the idea is we can use these definitions
** to set the appropriately sized native type.
**
** If you have problems on 64-bit machines and the code is 
** using a native datatype, please change the code to use
** these. (In the TDS layer only, the API layers have their
** own typedefs which equate to these).
*/
typedef char                           TDS_CHAR;      /*  8-bit char     */
typedef unsigned char                  TDS_UCHAR;     /*  8-bit uchar    */
typedef unsigned char                  TDS_TINYINT;   /*  8-bit unsigned */
typedef tds_sysdep_int16_type          TDS_SMALLINT;  /* 16-bit int      */
typedef unsigned tds_sysdep_int16_type TDS_USMALLINT; /* 16-bit unsigned */
typedef tds_sysdep_int32_type          TDS_INT;       /* 32-bit int      */
typedef unsigned tds_sysdep_int32_type TDS_UINT;      /* 32-bit unsigned */
typedef tds_sysdep_real32_type         TDS_REAL;      /* 32-bit real     */
typedef tds_sysdep_real64_type         TDS_FLOAT;     /* 64-bit real     */
typedef tds_sysdep_int64_type          TDS_INT8;      /* 64-bit integer  */
typedef unsigned tds_sysdep_int64_type TDS_UINT8;     /* 64-bit unsigned */

typedef struct tdsnumeric
{
        unsigned char         precision;
        unsigned char         scale;
        unsigned char         array[33];
} TDS_NUMERIC;

typedef struct tdsoldmoney
{
	TDS_INT         mnyhigh;
	TDS_UINT        mnylow;
} TDS_OLD_MONEY;

typedef union tdsmoney
{	TDS_OLD_MONEY	tdsoldmoney;
        TDS_INT8        mny;
} TDS_MONEY;

typedef struct tdsmoney4
{	
        TDS_INT          mny4;
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
    TDS_UINT      Data1;
    TDS_USMALLINT Data2;
    TDS_USMALLINT Data3;
    TDS_UCHAR     Data4[8];
} TDS_UNIQUE;

/** information on data, used by tds_datecrack */ 
typedef struct tdsdaterec
{
	TDS_INT   year;        /**< year */
	TDS_INT   month;       /**< month number (0-11) */
	TDS_INT   day;         /**< day of month (1-31) */
	TDS_INT   dayofyear;   /**< day of year  (1-366) */
	TDS_INT   weekday;     /**< day of week  (0-6, 0 = sunday) */
	TDS_INT   hour;        /**< 0-23 */
	TDS_INT   minute;      /**< 0-59 */
	TDS_INT   second;      /**< 0-59 */
	TDS_INT   millisecond; /**< 0-999 */
	TDS_INT   tzone;
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

enum tds_end {
	TDS_DONE_FINAL = 0,
	TDS_DONE_MORE_RESULTS = 1,
	TDS_DONE_ERROR = 2,
	TDS_DONE_COUNT = 16,
	TDS_DONE_CANCELLED = 32
};


/*
** TDS_ERROR indicates a successful processing, but an TDS_ERROR_TOKEN or 
** TDS_EED_TOKEN error was encountered, whereas TDS_FAIL indicates an
** unrecoverable failure.
*/
#define TDS_ERROR            3  
#define TDS_DONT_RETURN      42

#define TDS5_PARAMFMT2_TOKEN       32  /* 0x20 */
#define TDS_LANGUAGE_TOKEN         33  /* 0x21    TDS 5.0 only              */
#define TDS_ORDERBY2_TOKEN         34  /* 0x22 */
#define TDS_ROWFMT2_TOKEN          97  /* 0x61    TDS 5.0 only              */
#define TDS_LOGOUT_TOKEN          113  /* 0x71    TDS 5.0 only? ct_close()  */
#define TDS_RETURNSTATUS_TOKEN    121  /* 0x79                              */
#define TDS_PROCID_TOKEN          124  /* 0x7C    TDS 4.2 only - TDS_PROCID */
#define TDS7_RESULT_TOKEN         129  /* 0x81    TDS 7.0 only              */
#define TDS7_COMPUTE_RESULT_TOKEN 136  /* 0x88    TDS 7.0 only              */
#define TDS_COLNAME_TOKEN         160  /* 0xA0    TDS 4.2 only              */
#define TDS_COLFMT_TOKEN          161  /* 0xA1    TDS 4.2 only - TDS_COLFMT */
#define TDS_DYNAMIC2_TOKEN        163  /* 0xA3 */
#define TDS_TABNAME_TOKEN         164  /* 0xA4 */
#define TDS_COLINFO_TOKEN         165  /* 0xA5 */
#define TDS_OPTIONCMD_TOKEN   	  166  /* 0xA6 */
#define TDS_COMPUTE_NAMES_TOKEN   167  /* 0xA7 */
#define TDS_COMPUTE_RESULT_TOKEN  168  /* 0xA8 */
#define TDS_ORDERBY_TOKEN         169  /* 0xA9    TDS_ORDER                 */
#define TDS_ERROR_TOKEN           170  /* 0xAA                              */
#define TDS_INFO_TOKEN            171  /* 0xAB                              */
#define TDS_PARAM_TOKEN           172  /* 0xAC    RETURNVALUE?              */
#define TDS_LOGINACK_TOKEN        173  /* 0xAD                              */
#define TDS_CONTROL_TOKEN         174  /* 0xAE    TDS_CONTROL               */
#define TDS_ROW_TOKEN             209  /* 0xD1                              */
#define TDS_CMP_ROW_TOKEN         211  /* 0xD3                              */
#define TDS5_PARAMS_TOKEN         215  /* 0xD7    TDS 5.0 only              */
#define TDS_CAPABILITY_TOKEN      226  /* 0xE2                              */
#define TDS_ENVCHANGE_TOKEN       227  /* 0xE3                              */
#define TDS_EED_TOKEN             229  /* 0xE5                              */
#define TDS_DBRPC_TOKEN           230  /* 0xE6                              */
#define TDS5_DYNAMIC_TOKEN        231  /* 0xE7    TDS 5.0 only              */
#define TDS5_PARAMFMT_TOKEN       236  /* 0xEC    TDS 5.0 only              */
#define TDS_AUTH_TOKEN            237  /* 0xED                              */
#define TDS_RESULT_TOKEN          238  /* 0xEE                              */
#define TDS_DONE_TOKEN            253  /* 0xFD    TDS_DONE                  */
#define TDS_DONEPROC_TOKEN        254  /* 0xFE    TDS_DONEPROC              */
#define TDS_DONEINPROC_TOKEN      255  /* 0xFF    TDS_DONEINPROC            */

/* states for tds_process_messages() */
#define PROCESS_ROWS    0
#define PROCESS_RESULTS 1
#define CANCEL_PROCESS  2
#define GOTO_1ST_ROW    3
#define LOGIN           4

/* environment type field */
#define TDS_ENV_DATABASE  1
#define TDS_ENV_LANG      2
#define TDS_ENV_CHARSET   3
#define TDS_ENV_PACKSIZE  4
#define TDS_ENV_LCID        5
#define TDS_ENV_SQLCOLLATION 7

/* string types */
#define TDS_NULLTERM -9

/* 
<rant> Sybase does an awful job of this stuff, non null ints of size 1 2 
and 4 have there own codes but nullable ints are lumped into INTN
sheesh! </rant>
*/
typedef enum {
	SYBCHAR = 47, 	/* 0x2F */
#define SYBCHAR	SYBCHAR
	SYBVARCHAR = 39, 	/* 0x27 */
#define SYBVARCHAR	SYBVARCHAR
	SYBINTN = 38, 	/* 0x26 */
#define SYBINTN	SYBINTN
	SYBINT1 = 48, 	/* 0x30 */
#define SYBINT1	SYBINT1
	SYBINT2 = 52, 	/* 0x34 */
#define SYBINT2	SYBINT2
	SYBINT4 = 56, 	/* 0x38 */
#define SYBINT4	SYBINT4
	SYBINT8 = 127, 	/* 0x7F */
#define SYBINT8	SYBINT8
	SYBFLT8 = 62, 	/* 0x3E */
#define SYBFLT8	SYBFLT8
	SYBDATETIME = 61, 	/* 0x3D */
#define SYBDATETIME	SYBDATETIME
	SYBBIT = 50, 	/* 0x32 */
#define SYBBIT	SYBBIT
	SYBTEXT = 35, 	/* 0x23 */
#define SYBTEXT	SYBTEXT
	SYBNTEXT = 99, 	/* 0x63 */
#define SYBNTEXT	SYBNTEXT
	SYBIMAGE = 34, 	/* 0x22 */
#define SYBIMAGE	SYBIMAGE
	SYBMONEY4 = 122, 	/* 0x7A */
#define SYBMONEY4	SYBMONEY4
	SYBMONEY = 60, 	/* 0x3C */
#define SYBMONEY	SYBMONEY
	SYBDATETIME4 = 58, 	/* 0x3A */
#define SYBDATETIME4	SYBDATETIME4
	SYBREAL = 59, 	/* 0x3B */
#define SYBREAL	SYBREAL
	SYBBINARY = 45, 	/* 0x2D */
#define SYBBINARY	SYBBINARY
	SYBVOID = 31, 	/* 0x1F */
#define SYBVOID	SYBVOID
	SYBVARBINARY = 37, 	/* 0x25 */
#define SYBVARBINARY	SYBVARBINARY
	SYBNVARCHAR = 103, 	/* 0x67 */
#define SYBNVARCHAR	SYBNVARCHAR
	SYBBITN = 104, 	/* 0x68 */
#define SYBBITN	SYBBITN
	SYBNUMERIC = 108, 	/* 0x6C */
#define SYBNUMERIC	SYBNUMERIC
	SYBDECIMAL = 106, 	/* 0x6A */
#define SYBDECIMAL	SYBDECIMAL
	SYBFLTN = 109, 	/* 0x6D */
#define SYBFLTN	SYBFLTN
	SYBMONEYN = 110, 	/* 0x6E */
#define SYBMONEYN	SYBMONEYN
	SYBDATETIMN = 111, 	/* 0x6F */
#define SYBDATETIMN	SYBDATETIMN
	XSYBCHAR = 175, 	/* 0xAF */
#define XSYBCHAR	XSYBCHAR
	XSYBVARCHAR = 167, 	/* 0xA7 */
#define XSYBVARCHAR	XSYBVARCHAR
	XSYBNVARCHAR = 231, 	/* 0xE7 */
#define XSYBNVARCHAR	XSYBNVARCHAR
	XSYBNCHAR = 239, 	/* 0xEF */
#define XSYBNCHAR	XSYBNCHAR
	XSYBVARBINARY = 165, 	/* 0xA5 */
#define XSYBVARBINARY	XSYBVARBINARY
	XSYBBINARY = 173, 	/* 0xAD */
#define XSYBBINARY	XSYBBINARY
	SYBLONGBINARY = 225, 	/* 0xE1 */
#define SYBLONGBINARY	SYBLONGBINARY
	SYBSINT1 = 64, 	/* 0x40 */
#define SYBSINT1	SYBSINT1
	SYBUINT2 = 65, 	/* 0x41 */
#define SYBUINT2	SYBUINT2
	SYBUINT4 = 66, 	/* 0x42 */
#define SYBUINT4	SYBUINT4
	SYBUINT8 = 67, 	/* 0x43 */
#define SYBUINT8	SYBUINT8

	SYBUNIQUE = 36, 	/* 0x24 */
#define SYBUNIQUE	SYBUNIQUE
	SYBVARIANT = 98, 	/* 0x62 */
#define SYBVARIANT	SYBVARIANT
} TDS_SERVER_TYPE;

#define SYBAOPCNT  0x4b
#define SYBAOPCNTU 0x4c
#define SYBAOPSUM  0x4d
#define SYBAOPSUMU 0x4e
#define SYBAOPAVG  0x4f
#define SYBAOPAVGU 0x50
#define SYBAOPMIN  0x51
#define SYBAOPMAX  0x52

/** 
 * options that can be sent with a TDS_OPTIONCMD token
 */
typedef enum {
	  TDS_OPT_SET = 1		/* Set an option. */
	, TDS_OPT_DEFAULT = 2		/* Set option to its default value. */
	, TDS_OPT_LIST = 3		/* Request current setting of a specific option. */
	, TDS_OPT_INFO = 4		/* Report current setting of a specific option. */
} TDS_OPTION_CMD;

typedef enum {
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

static const TDS_INT TDS_OPT_ARITHOVERFLOW = 0x01;
static const TDS_INT TDS_OPT_NUMERICTRUNC = 0x02;

enum TDS_OPT_DATEFIRST_CHOICE {
	  TDS_OPT_MONDAY= 1
	, TDS_OPT_TUESDAY= 2
	, TDS_OPT_WEDNESDAY= 3
	, TDS_OPT_THURSDAY= 4
	, TDS_OPT_FRIDAY= 5
	, TDS_OPT_SATURDAY= 6
	, TDS_OPT_SUNDAY= 7
};

enum TDS_OPT_DATEFORMAT_CHOICE {
	  TDS_OPT_FMTMDY = 1
	, TDS_OPT_FMTDMY = 2
	, TDS_OPT_FMTYMD = 3
	, TDS_OPT_FMTYDM = 4
	, TDS_OPT_FMTMYD = 5
	, TDS_OPT_FMTDYM = 6
};
enum TDS_OPT_ISOLATION_CHOICE {
	  TDS_OPT_LEVEL1 = 1
	, TDS_OPT_LEVEL3 = 3
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

#define is_result_token(x) (x==TDS_RESULT_TOKEN    || \
			x==TDS_ROWFMT2_TOKEN || \
			x==TDS7_RESULT_TOKEN    || \
			x==TDS_COLFMT_TOKEN    || \
			x==TDS_COLNAME_TOKEN)

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
#define is_nullable_type(x) (x==SYBINTN || \
			x==SYBFLTN      || \
			x==SYBDATETIMN  || \
			x==SYBVARCHAR   || \
			x==SYBVARBINARY	|| \
			x==SYBMONEYN	|| \
			x==SYBTEXT	|| \
			x==SYBNTEXT	|| \
			x==SYBBITN      || \
			x==SYBIMAGE)
#define is_blob_type(x) (x==SYBTEXT || x==SYBIMAGE || x==SYBNTEXT)
/* large type means it has a two byte size field */
///define is_large_type(x) (x>128)
#define is_numeric_type(x) (x==SYBNUMERIC || x==SYBDECIMAL)
#define is_unicode_type(x) (x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_collate_type(x) (x==XSYBVARCHAR || x==XSYBCHAR || x==SYBTEXT || x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_ascii_type(x) ( x==XSYBCHAR || x==XSYBVARCHAR || x==SYBTEXT || x==SYBCHAR || x==SYBVARCHAR)
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
#define TDS_STR_CONNTMOUT "connect timeout"
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
typedef union { void *p; int i; } tds_align_struct;
#define TDS_ALIGN_SIZE sizeof(tds_align_struct)

#define TDS_MAX_LOGIN_STR_SZ 30
typedef struct tds_login {
	DSTR server_name;
	int port;
	TDS_TINYINT  major_version; /* TDS version */
	TDS_TINYINT  minor_version; /* TDS version */
	int block_size; 
	DSTR language; /* ie us-english */
	DSTR server_charset; /*  ie iso_1 */
	TDS_INT connect_timeout;
	DSTR host_name;
	DSTR app_name;
	DSTR user_name;
	DSTR password;
	/* Ct-Library, DB-Library,  TDS-Library or ODBC */
	DSTR library;
	TDS_TINYINT bulk_copy; 
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted; 

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func)(long lHint);
	long longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	DSTR client_charset;
} TDSLOGIN;

typedef struct tds_connect_info {
	/* first part of structure is the same of login one */
	DSTR server_name; /**< server name (in freetds.conf) */
	int port;          /**< port of database service */
	TDS_TINYINT major_version;
	TDS_TINYINT minor_version;
	int block_size;
	DSTR language;
	DSTR server_charset;    /**< charset of server */
	TDS_INT connect_timeout;
	DSTR host_name;     /**< client hostname */
	DSTR app_name;
	DSTR user_name;     /**< account for login */
	DSTR password;      /**< password of account login */
	DSTR library;
	TDS_TINYINT bulk_copy;
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted;

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func)(long lHint);
	long longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	DSTR client_charset;

	DSTR ip_addr;     /**< ip of server */
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
} TDSCONNECTINFO;

typedef struct tds_locale {
        char *language;
        char *char_set;
        char *date_fmt;
} TDSLOCALE;

/** structure that hold information about blobs (like text or image).
 * current_row contain this structure.
 */
typedef struct tds_blob_info {
	TDS_CHAR *textvalue;
	TDS_CHAR textptr[16];
	TDS_CHAR timestamp[8];
} TDSBLOBINFO;

/** hold information for collate in TDS8
 */
typedef struct 
{
	TDS_USMALLINT   locale_id;  /* master..syslanguages.lcid */
	TDS_USMALLINT   flags;      
	TDS_UCHAR	charset_id; /* or zero */
} TDS8_COLLATION;

/* SF stands for "sort flag" */
#define TDS_SF_BIN                   (TDS_USMALLINT) 0x100
#define TDS_SF_WIDTH_INSENSITIVE     (TDS_USMALLINT) 0x080
#define TDS_SF_KATATYPE_INSENSITIVE  (TDS_USMALLINT) 0x040
#define TDS_SF_ACCENT_SENSITIVE      (TDS_USMALLINT) 0x020
#define TDS_SF_CASE_INSENSITIVE      (TDS_USMALLINT) 0x010


/* forward declaration */
typedef struct tdsiconvinfo TDSICONVINFO;
/**
 * Information relevant to libiconv.  The name is an iconv name, not 
 * the same as found in master..syslanguages. 
 */ 
typedef struct _tds_encoding 
{
	char name[64];
	unsigned char min_bytes_per_char;
	unsigned char max_bytes_per_char;
} TDS_ENCODING;

struct tdsiconvinfo
{
	TDS_ENCODING client_charset;
	TDS_ENCODING server_charset;
#if HAVE_ICONV
	iconv_t to_wire;   /* conversion from client charset to server's format */
	iconv_t from_wire; /* conversion from server's format to client charset */
#endif
};


enum {TDS_SYSNAME_SIZE= 512};
/** 
 * Metadata about columns in regular and compute rows 
 */ 
typedef struct tds_column_info {
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
	struct {
		TDS_SMALLINT column_type;	/**< type of data, saved from wire */
		TDS_INT column_size;
	} on_server;
	
	const TDSICONVINFO *iconv_info;	/**< refers to previously allocated iconv information */

	TDS_CHAR table_name[TDS_SYSNAME_SIZE];
	TDS_CHAR column_name[TDS_SYSNAME_SIZE];

	TDS_INT column_offset;		/**< offset into row buffer for store data */
	unsigned int column_nullable:1;
	unsigned int column_writeable:1;
	unsigned int column_identity:1;
	unsigned int column_key:1;
	unsigned int column_hidden:1;
	unsigned int column_output:1;
	TDS_UCHAR    column_collation[5];

	/* additional fields flags for compute results */
	TDS_TINYINT  column_operator;
	TDS_SMALLINT column_operand;

	/* FIXME this is data related, not column */
	/** size written in variable (ie: char, text, binary) */
	TDS_INT column_cur_size;

	/* related to binding or info stored by client libraries */
	/* FIXME find a best place to store these data, some are unused */
	TDS_SMALLINT column_bindtype;
	TDS_SMALLINT column_bindfmt;
	TDS_UINT column_bindlen;
	TDS_CHAR *column_nullbind;
	TDS_CHAR *column_varaddr;
	TDS_CHAR *column_lenbind;
	TDS_INT column_textpos;
	TDS_INT column_text_sqlgetdatapos;
} TDSCOLINFO;

typedef struct
{
	int tab_colnum;
	char db_name[256];	/* column name */
	TDS_SMALLINT db_minlen;
	TDS_SMALLINT db_maxlen;
	TDS_SMALLINT db_colcnt;	/* I dont know what this does */
	TDS_TINYINT db_type;
	struct {
		TDS_SMALLINT column_type;	/**< type of data, saved from wire */
		TDS_INT column_size;
	} on_server;
	const TDSICONVINFO *iconv_info;	/**< refers to previously allocated iconv information */
	TDS_SMALLINT db_usertype;
	TDS_TINYINT db_varint_size;
	TDS_INT db_length;	/* size of field according to database */
	TDS_TINYINT db_nullable;
	TDS_TINYINT db_status;
	TDS_SMALLINT db_offset;
	TDS_TINYINT db_default;
	TDS_TINYINT db_prec;
	TDS_TINYINT db_scale;
	TDS_SMALLINT db_flags;
	TDS_INT db_size;
	char db_collate[5];
	long data_size;
	TDS_TINYINT *data;
	int txptr_offset;
} BCP_COLINFO;


/** Hold information for any results */
typedef struct tds_result_info {
	/* TODO those fields can became a struct */
	TDS_SMALLINT  num_cols;
	TDSCOLINFO    **columns;
	TDS_INT       row_size;
	int           null_info_size;
	unsigned char *current_row;

	TDS_SMALLINT  rows_exist;
	TDS_INT       row_count;
	TDS_SMALLINT  computeid;
	TDS_TINYINT   more_results;
	TDS_TINYINT   *bycolumns;
	TDS_SMALLINT  by_cols;
} TDSRESULTINFO;

/* values for tds->state */
enum {
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

typedef struct tds_result_info TDSCOMPUTEINFO;

typedef TDSRESULTINFO TDSPARAMINFO;

typedef struct tds_msg_info {
      TDS_SMALLINT priv_msg_type;
      TDS_SMALLINT line_number;
      TDS_UINT msg_number;
      TDS_SMALLINT msg_state;
      TDS_SMALLINT msg_level;
      TDS_CHAR *server;
      TDS_CHAR *message;
      TDS_CHAR *proc_name;
      TDS_CHAR *sql_state;
} TDSMSGINFO;

/*
** This is the current environment as reported by the server
*/
typedef struct tds_env_info {
	int block_size;
	char *language;
	char *charset;
	char *database;
} TDSENVINFO;

typedef struct tds_dynamic {
	char id[30];
	int dyn_state;
	/** numeric id for mssql7+*/
	TDS_INT num_id;
	TDSPARAMINFO *res_info;
	TDSPARAMINFO *params;
} TDSDYNAMIC;

/* forward declaration */
typedef struct tds_context TDSCONTEXT;
typedef struct tds_socket  TDSSOCKET;

struct tds_context {
	TDSLOCALE *locale;
	void *parent;
	/* handler */
	int (*msg_handler)(TDSCONTEXT*, TDSSOCKET*, TDSMSGINFO*);
	int (*err_handler)(TDSCONTEXT*, TDSSOCKET*, TDSMSGINFO*);
};

enum { client2ucs2, client2server_singlebyte, ascii2server_metadata } TDS_ICONV_INFO_ENTRY;

struct tds_socket {
	/* fixed and connect time */
        int s;
	TDS_SMALLINT major_version;
	TDS_SMALLINT minor_version;
	/** version of product (Sybase/MS and full version) */
	TDS_UINT product_version;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	unsigned char broken_dates;
	/* in/out buffers */
	unsigned char *in_buf;
	unsigned char *out_buf;
	unsigned int in_buf_max;
	unsigned in_pos;
	unsigned out_pos;
	unsigned in_len;
	unsigned out_len;
	unsigned char out_flag;
	unsigned char last_packet;
	void *parent;
	/* info about current query. 
	 * Contain information in process, even normal results and compute.
	 * This pointer shouldn't be freed.
	 */
	TDSRESULTINFO *curr_resinfo;
	TDSRESULTINFO *res_info;
	TDS_INT        num_comp_info;
	TDSCOMPUTEINFO **comp_info;
        TDSPARAMINFO *param_info;
	TDS_TINYINT   has_status;
	TDS_INT       ret_status;
	TDS_TINYINT   state;
	int rows_affected;
	/* timeout stuff from Jeff */
	TDS_INT timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func)(long lHint);
	long longquery_param;
	time_t queryStarttime;
	TDSENVINFO *env;
	/* dynamic placeholder stuff */
	int num_dyns;
	TDSDYNAMIC *cur_dyn;
	TDSDYNAMIC **dyns;
	int emul_little_endian;
	char *date_fmt;
	TDSCONTEXT *tds_ctx;
	int iconv_info_count;
	TDSICONVINFO *iconv_info;

	/** config for login stuff. After login this field is NULL */
	TDSCONNECTINFO *connect_info;
	int spid;
	TDS_UCHAR collation[5];
	void (*env_chg_func)(TDSSOCKET *tds, int type, char *oldval, char *newval);
	int (*chkintr)(TDSSOCKET *tds);
	int (*hndlintr)(TDSSOCKET *tds);
};

void tds_set_longquery_handler(TDSLOGIN * tds_login, void (* longquery_func)(long), long longquery_param);
void tds_set_timeouts(TDSLOGIN *tds_login, int connect_timeout, int query_timeout, int longquery_timeout);
int tds_init_write_buf(TDSSOCKET *tds);
void tds_free_result_info(TDSRESULTINFO *info);
void tds_free_socket(TDSSOCKET *tds);
void tds_free_connect(TDSCONNECTINFO *connect_info);
void tds_free_all_results(TDSSOCKET *tds);
void tds_free_results(TDSRESULTINFO *res_info);
void tds_free_param_results(TDSPARAMINFO *param_info);
void tds_free_msg(TDSMSGINFO *msg_info);
int tds_put_n(TDSSOCKET *tds, const void *buf, int n);
int tds_put_string(TDSSOCKET *tds, const char *buf,int len);
int tds_put_int(TDSSOCKET *tds, TDS_INT i);
int tds_put_int8(TDSSOCKET *tds, TDS_INT8 i);
int tds_put_smallint(TDSSOCKET *tds, TDS_SMALLINT si);
int tds_put_tinyint(TDSSOCKET *tds, TDS_TINYINT ti);
int tds_put_byte(TDSSOCKET *tds, unsigned char c);
TDSRESULTINFO *tds_alloc_results(int num_cols);
TDSCOMPUTEINFO **tds_alloc_compute_results(TDS_INT *num_comp_results, TDSCOMPUTEINFO** ci, int num_cols, int by_cols);
TDSCONTEXT *tds_alloc_context(void);
void tds_free_context(TDSCONTEXT *locale);
TDSSOCKET *tds_alloc_socket(TDSCONTEXT *context, int bufsize);

/* config.c */
const TDS_COMPILETIME_SETTINGS* tds_get_compiletime_settings(void);
typedef void (*TDSCONFPARSE)(const char* option, const char* value, void *param);
int tds_read_conf_section(FILE *in, const char *section, TDSCONFPARSE tds_conf_parse, void *parse_param);
int tds_read_conf_file(TDSCONNECTINFO *connect_info,const char *server);
TDSCONNECTINFO *tds_read_config_info(TDSSOCKET *tds, TDSLOGIN *login, TDSLOCALE *locale);
void tds_fix_connect(TDSCONNECTINFO *connect_info);
void tds_config_verstr(const char *tdsver, TDSCONNECTINFO *connect_info);
void tds_lookup_host(const char *servername, char *ip);
int tds_set_interfaces_file_loc(const char *interfloc);

TDSLOCALE *tds_get_locale(void);
unsigned char *tds_alloc_row(TDSRESULTINFO *res_info);
unsigned char *tds_alloc_compute_row(TDSCOMPUTEINFO *res_info);
char *tds_alloc_get_string(TDSSOCKET *tds, int len); 
void tds_set_null(unsigned char *current_row, int column);
void tds_clr_null(unsigned char *current_row, int column);
int tds_get_null(unsigned char *current_row, int column);
unsigned char *tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass);
TDSDYNAMIC *tds_lookup_dynamic(TDSSOCKET *tds, char *id);
const char *tds_prtype(int token);



/* iconv.c */
void tds_iconv_open(TDSSOCKET *tds, char *charset);
void tds_iconv_close(TDSSOCKET *tds);
void tds7_srv_charset_changed(TDSSOCKET * tds, int lcid);
 
/* threadsafe.c */
char *tds_timestamp_str(char *str, int maxlen);
struct hostent *tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct hostent *tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct servent *tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen);
char *tds_get_homedir(void);

/* mem.c */
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param);
void tds_free_input_params(TDSDYNAMIC *dyn);
void tds_free_all_dynamic(TDSSOCKET *tds);
void tds_free_dynamic(TDSSOCKET *tds, TDSDYNAMIC *dyn);
TDSSOCKET *tds_realloc_socket(TDSSOCKET *tds, int bufsize);
void tds_free_compute_result(TDSCOMPUTEINFO *comp_info);
void tds_free_compute_results(TDSCOMPUTEINFO **comp_info, TDS_INT num_comp);
unsigned char *tds_alloc_param_row(TDSPARAMINFO *info,TDSCOLINFO *curparam);
char *tds_alloc_lookup_sqlstate(TDSSOCKET *tds, int msgnum);
TDSLOGIN *tds_alloc_login(void);
TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, const char *id);
void tds_free_login(TDSLOGIN *login);
TDSCONNECTINFO *tds_alloc_connect(TDSLOCALE *locale);
TDSLOCALE *tds_alloc_locale(void);
void tds_free_locale(TDSLOCALE *locale);

/* login.c */
int tds7_send_auth(TDSSOCKET *tds, const unsigned char *challenge);
void tds_set_packet(TDSLOGIN *tds_login, int packet_size);
void tds_set_port(TDSLOGIN *tds_login, int port);
void tds_set_passwd(TDSLOGIN *tds_login, const char *password);
void tds_set_bulk(TDSLOGIN *tds_login, TDS_TINYINT enabled);
void tds_set_user(TDSLOGIN *tds_login, const char *username);
void tds_set_app(TDSLOGIN *tds_login, const char *application);
void tds_set_host(TDSLOGIN *tds_login, const char *hostname);
void tds_set_library(TDSLOGIN *tds_login, const char *library);
void tds_set_server(TDSLOGIN *tds_login, const char *server);
void tds_set_client_charset(TDSLOGIN *tds_login, const char *charset);
void tds_set_language(TDSLOGIN *tds_login, const char *language);
void tds_set_version(TDSLOGIN *tds_login, short major_ver, short minor_ver);
void tds_set_capabilities(TDSLOGIN *tds_login, unsigned char *capabilities, int size);
int tds_connect(TDSSOCKET *tds, TDSCONNECTINFO *connect_info);

/* query.c */
int tds_submit_query(TDSSOCKET *tds, const char *query, TDSPARAMINFO *params);
int tds_submit_queryf(TDSSOCKET *tds, const char *queryf, ...);
int tds_submit_prepare(TDSSOCKET *tds, const char *query, const char *id, TDSDYNAMIC **dyn_out, TDSPARAMINFO * params);
int tds_submit_execute(TDSSOCKET *tds, TDSDYNAMIC *dyn);
int tds_send_cancel(TDSSOCKET *tds);
const char *tds_next_placeholders(const char *start);
int tds_count_placeholders(const char *query);
int tds_get_dynid(TDSSOCKET *tds, char **id);
int tds_submit_unprepare(TDSSOCKET *tds, TDSDYNAMIC *dyn);
int tds_submit_rpc(TDSSOCKET *tds, const char *rpc_name, TDSPARAMINFO *params);
int tds_quote_id(TDSSOCKET * tds, char* buffer, const char *id);
int tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len);
const char *tds_skip_quoted(const char *s);

/* token.c */
int tds_process_cancel(TDSSOCKET *tds);
void tds_swap_datatype(int coltype, unsigned char *buf);
int tds_get_token_size(int marker);
int tds_process_login_tokens(TDSSOCKET *tds);
void tds_add_row_column_size(TDSRESULTINFO * info, TDSCOLINFO * curcol);
int tds_process_simple_query(TDSSOCKET * tds, TDS_INT * result_type);
int tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG *tds_argument, TDS_INT *tds_argsize);
int tds_process_result_tokens(TDSSOCKET *tds, TDS_INT *result_type);
int tds_process_row_tokens(TDSSOCKET *tds, TDS_INT *rowtype, TDS_INT *computeid);
int tds_process_default_tokens(TDSSOCKET *tds, int marker);
int tds_process_trailing_tokens(TDSSOCKET *tds);
int tds_client_msg(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, int msgnum, int level, int state, int line, const char *message);

/* data.c */
void tds_set_param_type(TDSSOCKET * tds, TDSCOLINFO * curcol, TDS_SERVER_TYPE type);
void tds_set_column_type(TDSCOLINFO *curcol, int type);


/* tds_convert.c */
TDS_INT tds_datecrack(TDS_INT datetype, const void *di, TDSDATEREC *dr);
int tds_get_conversion_type(int srctype, int colsize);

/* write.c */
int tds_put_bulk_data(TDSSOCKET *tds, const unsigned char *buf, TDS_INT bufsize);
int tds_flush_packet(TDSSOCKET *tds);
int tds_put_buf(TDSSOCKET *tds, const unsigned char *buf, int dsize, int ssize);
int tds7_put_bcpcol(TDSSOCKET * tds, const BCP_COLINFO *bcpcol);

/* read.c */
unsigned char tds_get_byte(TDSSOCKET *tds);
void tds_unget_byte(TDSSOCKET *tds);
unsigned char tds_peek(TDSSOCKET *tds);
TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds);
TDS_INT tds_get_int(TDSSOCKET *tds);
int tds_get_string(TDSSOCKET *tds, int string_len, char *dest, int need);
int tds_get_char_data(TDSSOCKET * tds, char *dest, int size, TDSCOLINFO *curcol);
void *tds_get_n(TDSSOCKET *tds, void *dest, int n);
int tds_get_size_by_type(int servertype);
int tds_read_packet (TDSSOCKET *tds);

/* util.c */
void tds_set_parent(TDSSOCKET *tds, void *the_parent);
void *tds_get_parent(TDSSOCKET *tds);
void tds_ctx_set_parent(TDSCONTEXT *ctx, void *the_parent);
void *tds_ctx_get_parent(TDSCONTEXT *ctx);
int tds_swap_bytes(unsigned char *buf, int bytes);
int tds_version(TDSSOCKET *tds_socket, char *pversion_string);
void tdsdump_off(void);
void tdsdump_on(void);
int  tdsdump_open(const char *filename);
int tdsdump_append(void);
void tdsdump_close(void);
void tdsdump_dump_buf(const void *buf, int length);
void tdsdump_log(int dbg_lvl, const char *fmt, ...);
int tds_close_socket(TDSSOCKET *tds);

/* vstrbuild.c */
int tds_vstrbuild(char *buffer, int buflen, int *resultlen, char *text, int textlen, const char *formats, int formatlen, va_list ap);

/* numeric.c */
char *tds_money_to_string(const TDS_MONEY *money, char *s);
char *tds_numeric_to_string(const TDS_NUMERIC *numeric, char *s);

/* getmac.c */
void tds_getmac(int s, unsigned char mac[6]);

typedef struct tds_answer
{
	unsigned char lm_resp[24];
	unsigned char nt_resp[24];
} TDSANSWER;
void tds_answer_challenge(const char *passwd, const unsigned char *challenge,TDSANSWER* answer);

#define IS_TDS42(x) (x->major_version==4 && x->minor_version==2)
#define IS_TDS46(x) (x->major_version==4 && x->minor_version==6)
#define IS_TDS50(x) (x->major_version==5 && x->minor_version==0)
#define IS_TDS70(x) (x->major_version==7 && x->minor_version==0)
#define IS_TDS80(x) (x->major_version==8 && x->minor_version==0)

#define IS_TDS7_PLUS(x) ( IS_TDS70(x) || IS_TDS80(x) )

#define IS_TDSDEAD(x) (((x) == NULL) || ((x)->s < 0))

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
}
#endif 

#endif /* _tds_h_ */
