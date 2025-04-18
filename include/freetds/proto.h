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

/*
 * This file contains defines and structures strictly related to TDS protocol
 */

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

typedef struct tdsunique
{
	TDS_UINT Data1;
	TDS_USMALLINT Data2;
	TDS_USMALLINT Data3;
	TDS_UCHAR Data4[8];
} TDS_UNIQUE;

typedef TDS_INT TDS_DATE;
typedef TDS_INT TDS_TIME;

typedef TDS_UINT8 TDS_BIGTIME;
typedef TDS_UINT8 TDS_BIGDATETIME;

#define TDS5_PARAMFMT2_TOKEN       32	/* 0x20 */
#define TDS_LANGUAGE_TOKEN         33	/* 0x21    TDS 5.0 only              */
#define TDS_ORDERBY2_TOKEN         34	/* 0x22 */
#define TDS_ROWFMT2_TOKEN          97	/* 0x61    TDS 5.0 only              */
#define TDS_MSG_TOKEN             101	/* 0x65    TDS 5.0 only              */
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
#define TDS_CONTROL_FEATUREEXTACK_TOKEN \
				  174	/* 0xAE    TDS_CONTROL/TDS_FEATUREEXTACK */
#define TDS_ROW_TOKEN             209	/* 0xD1                              */
#define TDS_NBC_ROW_TOKEN         210	/* 0xD2    as of TDS 7.3.B           */
#define TDS_CMP_ROW_TOKEN         211	/* 0xD3                              */
#define TDS5_PARAMS_TOKEN         215	/* 0xD7    TDS 5.0 only              */
#define TDS_CAPABILITY_TOKEN      226	/* 0xE2                              */
#define TDS_ENVCHANGE_TOKEN       227	/* 0xE3                              */
#define TDS_SESSIONSTATE_TOKEN    228	/* 0xE4    TDS 7.4                   */
#define TDS_EED_TOKEN             229	/* 0xE5                              */
#define TDS_DBRPC_TOKEN           230	/* 0xE6    TDS 5.0 only              */
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
#define TDS_ENV_ROUTING 	20

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

/**
 * Flags returned in TDS_DONE token
 */
enum {
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
 * <rant> Sybase does an awful job of this stuff, non null ints of size 1 2 
 * and 4 have there own codes but nullable ints are lumped into INTN
 * sheesh! </rant>
 */
typedef enum
{
	SYBCHAR = 47,		/* 0x2F */
	SYBVARCHAR = 39,	/* 0x27 */
	SYBINTN = 38,		/* 0x26 */
	SYBINT1 = 48,		/* 0x30 */
	SYBINT2 = 52,		/* 0x34 */
	SYBINT4 = 56,		/* 0x38 */
	SYBFLT8 = 62,		/* 0x3E */
	SYBDATETIME = 61,	/* 0x3D */
	SYBBIT = 50,		/* 0x32 */
	SYBTEXT = 35,		/* 0x23 */
	SYBNTEXT = 99,		/* 0x63 */
	SYBIMAGE = 34,		/* 0x22 */
	SYBMONEY4 = 122,	/* 0x7A */
	SYBMONEY = 60,		/* 0x3C */
	SYBDATETIME4 = 58,	/* 0x3A */
	SYBREAL = 59,		/* 0x3B */
	SYBBINARY = 45,		/* 0x2D */
	SYBVOID = 31,		/* 0x1F */
	SYBVARBINARY = 37,	/* 0x25 */
	SYBBITN = 104,		/* 0x68 */
	SYBNUMERIC = 108,	/* 0x6C */
	SYBDECIMAL = 106,	/* 0x6A */
	SYBFLTN = 109,		/* 0x6D */
	SYBMONEYN = 110,	/* 0x6E */
	SYBDATETIMN = 111,	/* 0x6F */

/*
 * MS only types
 */
	SYBINT8 = 127,		/* 0x7F */
	XSYBCHAR = 175,		/* 0xAF */
	XSYBVARCHAR = 167,	/* 0xA7 */
	XSYBNVARCHAR = 231,	/* 0xE7 */
	XSYBNCHAR = 239,	/* 0xEF */
	XSYBVARBINARY = 165,	/* 0xA5 */
	XSYBBINARY = 173,	/* 0xAD */
	SYBUNIQUE = 36,		/* 0x24 */
	SYBVARIANT = 98, 	/* 0x62 */
	SYBMSUDT = 240,		/* 0xF0 */
	SYBMSXML = 241,		/* 0xF1 */
	SYBMSDATE = 40,  	/* 0x28 */
	SYBMSTIME = 41,  	/* 0x29 */
	SYBMSDATETIME2 = 42,  	/* 0x2a */
	SYBMSDATETIMEOFFSET = 43,/* 0x2b */
	SYBMSTABLE = 243,	/* 0xF3 */

/*
 * Sybase only types
 */
	SYBNVARCHAR = 103,	/* 0x67 */
	SYBLONGBINARY = 225,	/* 0xE1 */
	SYBUINT1 = 64,		/* 0x40 */
	SYBUINT2 = 65,		/* 0x41 */
	SYBUINT4 = 66,		/* 0x42 */
	SYBUINT8 = 67,		/* 0x43 */
	SYBBLOB = 36,		/* 0x24 */
	SYBBOUNDARY = 104,	/* 0x68 */
	SYBDATE = 49,		/* 0x31 */
	SYBDATEN = 123,		/* 0x7B */
	SYB5INT8 = 191,		/* 0xBF */
	SYBINTERVAL = 46,	/* 0x2E */
	SYBLONGCHAR = 175,	/* 0xAF */
	SYBSENSITIVITY = 103,	/* 0x67 */
	SYBSINT1 = 176,		/* 0xB0 */
	SYBTIME = 51,		/* 0x33 */
	SYBTIMEN = 147,		/* 0x93 */
	SYBUINTN = 68,		/* 0x44 */
	SYBUNITEXT = 174,	/* 0xAE */
	SYBXML = 163,		/* 0xA3 */
	SYB5BIGDATETIME = 187,	/* 0xBB */
	SYB5BIGTIME = 188,	/* 0xBC */

} TDS_SERVER_TYPE;

typedef enum
{
	USER_CHAR_TYPE = 1,		/* 0x01 */
	USER_VARCHAR_TYPE = 2,		/* 0x02 */
	USER_SYSNAME_TYPE = 18,		/* 0x12 */
	USER_NCHAR_TYPE = 24,		/* 0x18 */
	USER_NVARCHAR_TYPE = 25,	/* 0x19 */
	USER_UNICHAR_TYPE = 34,		/* 0x22 */
	USER_UNIVARCHAR_TYPE = 35,	/* 0x23 */
	USER_UNITEXT_TYPE = 36,		/* 0x24 */
} TDS_USER_TYPE;

/* compute operator */
#define SYBAOPCNT  75		/* 0x4B */
#define SYBAOPCNTU 76		/* 0x4C, obsolete */
#define SYBAOPSUM  77		/* 0x4D */
#define SYBAOPSUMU 78		/* 0x4E, obsolete */
#define SYBAOPAVG  79		/* 0x4F */
#define SYBAOPAVGU 80		/* 0x50, obsolete */
#define SYBAOPMIN  81		/* 0x51 */
#define SYBAOPMAX  82		/* 0x52 */

/* mssql2k compute operator */
#define SYBAOPCNT_BIG		9	/* 0x09 */
#define SYBAOPSTDEV		48	/* 0x30 */
#define SYBAOPSTDEVP		49	/* 0x31 */
#define SYBAOPVAR		50	/* 0x32 */
#define SYBAOPVARP		51	/* 0x33 */
#define SYBAOPCHECKSUM_AGG	114	/* 0x72 */

/** 
 * options that can be sent with a TDS_OPTIONCMD token
 */
typedef enum
{
	  TDS_OPT_SET = 1	/**< Set an option. */
	, TDS_OPT_DEFAULT = 2	/**< Set option to its default value. */
	, TDS_OPT_LIST = 3	/**< Request current setting of a specific option. */
	, TDS_OPT_INFO = 4	/**< Report current setting of a specific option. */
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
	TDS_OPT_LEVEL0 = 0,
	TDS_OPT_LEVEL1 = 1,
	TDS_OPT_LEVEL2 = 2,
	TDS_OPT_LEVEL3 = 3
};


typedef enum tds_packet_type
{
	TDS_QUERY = 1,
	TDS_LOGIN = 2,
	TDS_RPC = 3,
	TDS_REPLY = 4,
	TDS_CANCEL = 6,
	TDS_BULK = 7,
	TDS7_TRANS = 14,	/* transaction management */
	TDS_NORMAL = 15,
	TDS7_LOGIN = 16,
	TDS7_AUTH = 17,
	TDS71_PRELOGIN = 18,
	TDS72_SMP = 0x53
} TDS_PACKET_TYPE;

/** 
 * TDS 7.1 collation information.
 */
typedef struct
{
	TDS_USMALLINT locale_id;	/* master..syslanguages.lcid */
	TDS_USMALLINT flags;
	TDS_UCHAR charset_id;		/* or zero */
} TDS71_COLLATION;

/**
 * TDS packet header
 */
typedef struct
{
	TDS_UCHAR type;
	TDS_UCHAR status;
	TDS_USMALLINT length;
	TDS_USMALLINT spid;
	TDS_UCHAR packet_id;
	TDS_UCHAR window;
} TDS_HEADER;

enum {
	TDS_STATUS_EOM = 1,
	TDS_STATUS_RESETCONNECTION = 8,
};

/**
 * TDS 7.2 SMP packet header
 */
typedef struct
{
	TDS_UCHAR signature;	/* TDS72_SMP */
	TDS_UCHAR type;
	TDS_USMALLINT sid;
	TDS_UINT size;
	TDS_UINT seq;
	TDS_UINT wnd;
} TDS72_SMP_HEADER;

enum {
	TDS_SMP_SYN = 1,
	TDS_SMP_ACK = 2,
	TDS_SMP_FIN = 4,
	TDS_SMP_DATA = 8,
};

/* SF stands for "sort flag" */
#define TDS_SF_BIN                   (TDS_USMALLINT) 0x100
#define TDS_SF_WIDTH_INSENSITIVE     (TDS_USMALLINT) 0x080
#define TDS_SF_KATATYPE_INSENSITIVE  (TDS_USMALLINT) 0x040
#define TDS_SF_ACCENT_SENSITIVE      (TDS_USMALLINT) 0x020
#define TDS_SF_CASE_INSENSITIVE      (TDS_USMALLINT) 0x010

/* UT stands for user type */
#define TDS_UT_TIMESTAMP             80


/* mssql login options flags */
enum option_flag1_values {
	TDS_BYTE_ORDER_X86		= 0, 
	TDS_CHARSET_ASCII		= 0, 
	TDS_DUMPLOAD_ON 		= 0, 
	TDS_FLOAT_IEEE_754		= 0, 
	TDS_INIT_DB_WARN		= 0, 
	TDS_SET_LANG_OFF		= 0, 
	TDS_USE_DB_SILENT		= 0, 
	TDS_BYTE_ORDER_68000	= 0x01, 
	TDS_CHARSET_EBDDIC		= 0x02, 
	TDS_FLOAT_VAX		= 0x04, 
	TDS_FLOAT_ND5000		= 0x08, 
	TDS_DUMPLOAD_OFF		= 0x10,	/* prevent BCP */ 
	TDS_USE_DB_NOTIFY		= 0x20, 
	TDS_INIT_DB_FATAL		= 0x40, 
	TDS_SET_LANG_ON		= 0x80
};

enum option_flag2_values {
	TDS_INIT_LANG_WARN		= 0, 
	TDS_INTEGRATED_SECURTY_OFF	= 0, 
	TDS_ODBC_OFF		= 0, 
	TDS_USER_NORMAL		= 0,	/* SQL Server login */
	TDS_INIT_LANG_REQUIRED	= 0x01, 
	TDS_ODBC_ON			= 0x02, 
	TDS_TRANSACTION_BOUNDARY71	= 0x04,	/* removed in TDS 7.2 */
	TDS_CACHE_CONNECT71		= 0x08,	/* removed in TDS 7.2 */
	TDS_USER_SERVER		= 0x10,	/* reserved */
	TDS_USER_REMUSER		= 0x20,	/* DQ login */
	TDS_USER_SQLREPL		= 0x40,	/* replication login */
	TDS_INTEGRATED_SECURITY_ON	= 0x80
};

enum option_flag3_values {
	TDS_RESTRICTED_COLLATION	= 0, 
	TDS_CHANGE_PASSWORD		= 0x01, /* TDS 7.2 */
	TDS_SEND_YUKON_BINARY_XML	= 0x02, /* TDS 7.2 */
	TDS_REQUEST_USER_INSTANCE	= 0x04, /* TDS 7.2 */
	TDS_UNKNOWN_COLLATION_HANDLING	= 0x08, /* TDS 7.3 */
	TDS_EXTENSION			= 0x10, /* TDS 7.4 */
};

enum type_flags {
	TDS_OLEDB_ON	= 0x10,
	TDS_READONLY_INTENT	= 0x20,
};

/* Sybase dynamic types */
enum dynamic_types {
	TDS_DYN_PREPARE		= 0x01,
	TDS_DYN_EXEC		= 0x02,
	TDS_DYN_DEALLOC		= 0x04,
	TDS_DYN_EXEC_IMMED	= 0x08,
	TDS_DYN_PROCNAME	= 0x10,
	TDS_DYN_ACK		= 0x20,
	TDS_DYN_DESCIN		= 0x40,
	TDS_DYN_DESCOUT		= 0x80,
};

/* http://jtds.sourceforge.net/apiCursors.html */
/* Cursor scroll option, must be one of 0x01 - 0x10, OR'd with other bits */
enum {
	TDS_CUR_TYPE_KEYSET                  = 0x0001, /* default */
	TDS_CUR_TYPE_DYNAMIC                 = 0x0002,
	TDS_CUR_TYPE_FORWARD                 = 0x0004,
	TDS_CUR_TYPE_STATIC                  = 0x0008,
	TDS_CUR_TYPE_FASTFORWARDONLY         = 0x0010,
	TDS_CUR_TYPE_PARAMETERIZED           = 0x1000,
	TDS_CUR_TYPE_AUTO_FETCH              = 0x2000,
	TDS_CUR_TYPE_AUTO_CLOSE              = 0x4000,
	TDS_CUR_TYPE_CHECK_ACCEPTED_TYPES    = 0x8000,
	TDS_CUR_TYPE_KEYSET_ACCEPTABLE       = 0x10000,
	TDS_CUR_TYPE_DYNAMIC_ACCEPTABLE      = 0x20000,
	TDS_CUR_TYPE_FORWARD_ONLY_ACCEPTABLE = 0x40000,
	TDS_CUR_TYPE_STATIC_ACCEPTABLE       = 0x80000,
	TDS_CUR_TYPE_FAST_FORWARD_ACCEPTABLE = 0x100000
};

enum {
	TDS_CUR_CONCUR_READ_ONLY               = 1,
	TDS_CUR_CONCUR_SCROLL_LOCKS            = 2,
	TDS_CUR_CONCUR_OPTIMISTIC              = 4, /* default */
	TDS_CUR_CONCUR_OPTIMISTIC_VALUES       = 8,
	TDS_CUR_CONCUR_ALLOW_DIRECT            = 0x2000,
	TDS_CUR_CONCUR_UPDATE_IN_PLACE         = 0x4000,
	TDS_CUR_CONCUR_CHECK_ACCEPTED_OPTS     = 0x8000,
	TDS_CUR_CONCUR_READ_ONLY_ACCEPTABLE    = 0x10000,
	TDS_CUR_CONCUR_SCROLL_LOCKS_ACCEPTABLE = 0x20000,
	TDS_CUR_CONCUR_OPTIMISTIC_ACCEPTABLE   = 0x40000
};

/* TDS 4/5 login*/
#define TDS_MAXNAME 30	/* maximum login name lenghts */
#define TDS_PROGNLEN 10	/* maximum program lenght */
#define TDS_PKTLEN 6	/* maximum packet lenght in login */

/* TDS 5 login security flags */
enum {
	TDS5_SEC_LOG_ENCRYPT = 1,
	TDS5_SEC_LOG_CHALLENGE = 2,
	TDS5_SEC_LOG_LABELS = 4,
	TDS5_SEC_LOG_APPDEFINED = 8,
	TDS5_SEC_LOG_SECSESS = 16,
	TDS5_SEC_LOG_ENCRYPT2 = 32,
	TDS5_SEC_LOG_ENCRYPT3 = 128,
};

/** TDS 5 TDS_MSG_TOKEN message types */
enum {
	TDS5_MSG_SEC_ENCRYPT = 1, /**< Start encrypted login protocol. */
	TDS5_MSG_SEC_LOGPWD = 2, /**< Sending encrypted user password. */
	TDS5_MSG_SEC_REMPWD = 3, /**< Sending remote server passwords. */
	TDS5_MSG_SEC_CHALLENGE = 4, /**< Start challenge/response protocol. */
	TDS5_MSG_SEC_RESPONSE = 5, /**< Returned encrypted challenge. */
	TDS5_MSG_SEC_GETLABEL = 6, /**< Start trusted user login protocol. */
	TDS5_MSG_SEC_LABEL = 7, /**< Return security labels. */
	TDS5_MSG_SQL_TBLNAME = 8, /**< CS_MSG_TABLENAME */
	TDS5_MSG_GW_RESERVED = 9, /**< Used by interoperability group. */
	TDS5_MSG_OMNI_CAPABILITIES = 10, /**< Used by OMNI SQL Server. */
	TDS5_MSG_SEC_OPAQUE = 11, /**< Send opaque security token. */
	TDS5_MSG_HAFAILOVER = 12, /**< Used during login to obtain the HA Session ID */
	TDS5_MSG_EMPTY = 13, /**< Sometimes a MSG response stream is required by TDS syntax,
			but the sender has no real information to pass. This message type
			indicates that the following paramfmt/param streams are meaningless */
	TDS5_MSG_SEC_ENCRYPT2 = 14, /**< Start alternate encrypted password protocol. */
	TDS5_MSG_SEC_LOGPWD2 = 15, /**< Return alternate encrypted passwords. */
	TDS5_MSG_SEC_SUP_CIPHER = 16, /**< Returns list of supported ciphers. */
	TDS5_MSG_MIG_REQ = 17, /**< Initiate client connection migration to alternative
			server via address pro- vided as message parameter. */
	TDS5_MSG_MIG_SYNC = 18, /**< Client sends to acknowledge receipt of TDS_MSG_MIG_REQ . */
	TDS5_MSG_MIG_CONT = 19, /**< Server sends to start actual client migration to alternate server. */
	TDS5_MSG_MIG_IGN = 20, /**< Server sends to abort previous TDS_MSG_MIG_REQ . */
	TDS5_MSG_MIG_FAIL = 21, /**< Client sends to original server to indicate that the
			migration attempt failed. Optional parameter indicates failure reason. */
	TDS5_MSG_SEC_REMPWD2 = 22,
	TDS5_MSG_MIG_RESUME = 23,
	TDS5_MSG_SEC_ENCRYPT3 = 30,
	TDS5_MSG_SEC_LOGPWD3 = 31,
	TDS5_MSG_SEC_REMPWD3 = 32,
	TDS5_MSG_DR_MAP = 33,
};

/**
 * TDS 5 TDS5_MSG_SEC_OPAQUE types.
 *
 * TDS5_SEC_SECSESS has 5 parameters
 * 1- security version. INTN(4). Always TDS5_SEC_VERSION
 * 2- security message type. INTN(4). Always TDS5_SEC_SECSESS
 * 3- security OID. VARBINARY.
 * 4- opaque security token. LONGVARBINARY.
 * 5- security services requested. INTN(4). A set of flags.
 */
enum {
	TDS5_SEC_SECSESS = 1, /**< Security session token */
	TDS5_SEC_FORWARD = 2, /**< Credential forwarding */
	TDS5_SEC_SIGN = 3, /**< Data signature packet */
	TDS5_SEC_OTHER = 4, /**< Other security message */
};

/**
 * TDS 5 security services
 */
enum {
	TDS5_SEC_NETWORK_AUTHENTICATION = 0x1,
	TDS5_SEC_MUTUAL_AUTHENTICATION = 0x2,
	TDS5_SEC_DELEGATION = 0x4,
	TDS5_SEC_INTEGRITY = 0x8,
	TDS5_SEC_CONFIDENTIALITY = 0x10,
	TDS5_SEC_DETECT_REPLAY = 0x20,
	TDS5_SEC_DETECT_SEQUENCE = 0x40,
	TDS5_SEC_DATA_ORIGIN = 0x80,
	TDS5_SEC_CHANNEL_BINDING = 0x100,
};

enum {
	TDS5_SEC_VERSION = 50,
};

/* MS encryption byte (pre login) */
enum {
	TDS7_ENCRYPT_OFF,
	TDS7_ENCRYPT_ON,
	TDS7_ENCRYPT_NOT_SUP,
	TDS7_ENCRYPT_REQ,
};
