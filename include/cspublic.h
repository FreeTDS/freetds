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

#ifndef _cspublic_h_
#define _cspublic_h_

#include <tds.h>

#ifdef __cplusplus
extern "C" {
#endif 

static char  rcsid_cspublic_h [ ] =
         "$Id: cspublic.h,v 1.30 2003-02-10 22:06:59 jklowden Exp $";
static void *no_unused_cspublic_h_warn[]={rcsid_cspublic_h, no_unused_cspublic_h_warn};

typedef int CS_RETCODE ;

#define CS_PUBLIC 
#define CS_STATIC static

typedef TDS_INT CS_INT;
typedef TDS_SMALLINT CS_SMALLINT;
typedef TDS_TINYINT CS_TINYINT;
typedef TDS_CHAR CS_CHAR;
typedef TDS_UCHAR CS_BYTE;
typedef TDS_NUMERIC CS_NUMERIC;
typedef float CS_REAL;
typedef double CS_FLOAT;
typedef char CS_BOOL;
typedef void CS_VOID;
typedef TDS_VARBINARY CS_VARBINARY;
typedef TDS_NUMERIC CS_DECIMAL;
typedef TDS_UCHAR CS_IMAGE;
typedef TDS_UCHAR CS_TEXT;
typedef TDS_UCHAR CS_LONGBINARY;
typedef TDS_UCHAR CS_LONGCHAR;
typedef TDS_INT CS_LONG;
typedef TDS_UCHAR CS_BINARY;
typedef TDS_USMALLINT CS_USHORT;
typedef TDS_UCHAR CS_BIT;

#define CS_FAIL	   TDS_FAIL
#define CS_SUCCEED TDS_SUCCEED
#define CS_SIZEOF(x) sizeof(x)

#define CS_LAYER(x)    (((x) >> 24) & 0xFF)
#define CS_ORIGIN(x)   (((x) >> 16) & 0xFF)
#define CS_SEVERITY(x) (((x) >>  8) & 0xFF)
#define CS_NUMBER(x)   ((x) & 0xFF)

#define CS_OBJ_NAME 132 /* ? */
#define CS_TP_SIZE  16  /* text pointer */
#define CS_TS_SIZE  8   /* length of timestamp */

typedef struct cs_config
{
    short cs_expose_formats;
} CS_CONFIG;

/* forward declarations */
typedef struct cs_context CS_CONTEXT;
typedef struct cs_clientmsg CS_CLIENTMSG;
typedef struct cs_connection CS_CONNECTION;
typedef struct cs_servermsg CS_SERVERMSG;
typedef CS_RETCODE (*CS_CSLIBMSG_FUNC)(CS_CONTEXT *, CS_CLIENTMSG *);
typedef CS_RETCODE (*CS_CLIENTMSG_FUNC)(CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *);
typedef CS_RETCODE (*CS_SERVERMSG_FUNC)(CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *);

struct cs_context
{
	CS_INT date_convert_fmt;
	CS_CSLIBMSG_FUNC _cslibmsg_cb;
	CS_CLIENTMSG_FUNC _clientmsg_cb;
	CS_SERVERMSG_FUNC _servermsg_cb;
	TDSCONTEXT *tds_ctx;
    CS_CONFIG config;
};

typedef struct cs_locale {
	char *language;
	char *charset;
	char *time;
	char *collate;
} CS_LOCALE;

struct cs_connection
{
	CS_CONTEXT *ctx;
	TDSLOGIN *tds_login;
	TDSSOCKET *tds_socket;
	CS_CLIENTMSG_FUNC _clientmsg_cb;
	CS_SERVERMSG_FUNC _servermsg_cb;
	void *userdata;
	int userdata_len;
	CS_LOCALE *locale;
};


#define CS_IODATA          (CS_INT)1600

typedef struct cs_iodesc {
	CS_INT	iotype;
	CS_INT	datatype;
	CS_LOCALE	*locale;
	CS_INT	usertype;
	CS_INT	total_txtlen;
	CS_INT	offset;
	CS_BOOL	log_on_update;
	CS_CHAR	name[CS_OBJ_NAME];
	CS_INT	namelen;
	CS_BYTE textptr[CS_TP_SIZE];
	CS_INT	textptrlen;
	CS_BYTE timestamp[CS_TS_SIZE];
	CS_INT	timestamplen;
} CS_IODESC;


typedef struct cs_command
{
	CS_CHAR *query;
	CS_INT   command_type;
	CS_CONNECTION *con;
	short dynamic_cmd;
	char  *dyn_id; 
    int   row_prefetched;
    int   empty_result;
    int   curr_result_type;
	int   get_data_item;
	int   get_data_bytes_returned;
	CS_IODESC *iodesc;
	CS_INT send_data_started;
} CS_COMMAND;

#define CS_MAX_MSG 1024
#define CS_MAX_NAME 132
#define CS_MAX_SCALE 77
#define CS_MAX_PREC 77  /* used by php */
#define CS_SQLSTATE_SIZE 8


#define CS_SRC_VALUE   -2562

typedef struct cs_datafmt {
	int datatype;
	int format;
	int maxlength;
	int count;
	CS_LOCALE *locale;
	int precision;
	int scale;
	int namelen;
	char name[CS_MAX_NAME];
	int status;
	int usertype;
} CS_DATAFMT;

typedef TDS_MONEY  CS_MONEY;
typedef TDS_MONEY4 CS_MONEY4;

typedef TDS_DATETIME CS_DATETIME;

typedef TDS_DATETIME4 CS_DATETIME4;

typedef struct cs_daterec {
	CS_INT datesecond;
	CS_INT dateminute;
	CS_INT datehour;
	CS_INT datedmonth;
	CS_INT datedyear;
	CS_INT datemonth;
	CS_INT dateyear;
	CS_INT dateweek;
	CS_INT datedweek;
	CS_INT datemsecond;
	CS_INT datetzone;
} CS_DATEREC;

typedef TDS_INT CS_MSGNUM;

struct cs_clientmsg {
	CS_INT severity;
	CS_MSGNUM msgnumber;
	CS_CHAR msgstring[CS_MAX_MSG];
	CS_INT msgstringlen;
	CS_INT osnumber;
	CS_CHAR osstring[CS_MAX_MSG]; 
	CS_INT osstringlen;
	CS_INT status;
	CS_BYTE sqlstate[CS_SQLSTATE_SIZE];
	CS_INT sqlstatelen;
};

struct cs_servermsg {
	int severity;
	int msgnumber;
	int state;
	int line;
	int svrnlen;
	char svrname[CS_MAX_NAME];
	int proclen;
	char proc[CS_MAX_NAME];
	char text[CS_MAX_MSG];
	int status;
};

/* status bits for CS_SERVERMSG */
#define CS_HASEED 0x01

typedef struct cs_blkdesc {
	int dummy;	
} CS_BLKDESC;

/* CS_CAP_REQUEST values */
#define CS_REQ_LANG	1
#define CS_REQ_RPC	2
#define CS_REQ_NOTIF	3
#define CS_REQ_MSTMT	4
#define CS_REQ_BCP	5
#define CS_REQ_CURSOR	6
#define CS_REQ_DYN	7
#define CS_REQ_MSG	8
#define CS_REQ_PARAM	9
#define CS_DATA_INT1	10
#define CS_DATA_INT2	11
#define CS_DATA_INT4	12
#define CS_DATA_BIT	13
#define CS_DATA_CHAR	14
#define CS_DATA_VCHAR	15
#define CS_DATA_BIN	16
#define CS_DATA_VBIN	17
#define CS_DATA_MNY8	18
#define CS_DATA_MNY4	19
#define CS_DATA_DATE8	20
#define CS_DATA_DATE4	21
#define CS_DATA_FLT4	22
#define CS_DATA_FLT8	23
#define CS_DATA_NUM	24
#define CS_DATA_TEXT	25
#define CS_DATA_IMAGE	26
#define CS_DATA_DEC	27
#define CS_DATA_LCHAR	28
#define CS_DATA_LBIN	29
#define CS_DATA_INTN	30
#define CS_DATA_DATETIMEN	31
#define CS_DATA_MONEYN	32
#define CS_CSR_PREV	33
#define CS_CSR_FIRST	34
#define CS_CSR_LAST	35
#define CS_CSR_ABS	36
#define CS_CSR_REL	37
#define CS_CSR_MULTI	38
#define CS_CON_OOB	39
#define CS_CON_INBAND	40
#define CS_CON_LOGICAL	41
#define CS_PROTO_TEXT	42
#define CS_PROTO_BULK	43
#define CS_REQ_URGNOTIF	44
#define CS_DATA_SENSITIVITY	45
#define CS_DATA_BOUNDARY	46
#define CS_PROTO_DYNAMIC	47
#define CS_PROTO_DYNPROC	48
#define CS_DATA_FLTN	49
#define CS_DATA_BITN	50
#define CS_DATA_INT8	51
#define CS_DATA_VOID	52
#define CS_OPTION_GET	53

/* CS_CAP_RESPONSE values */
#define CS_RES_NOMSG	1
#define CS_RES_NOEED	2
#define CS_RES_NOPARAM	3
#define CS_DATA_NOINT1	4
#define CS_DATA_NOINT2	5
#define CS_DATA_NOINT4	6
#define CS_DATA_NOBIT	7
#define CS_DATA_NOCHAR	8
#define CS_DATA_NOVCHAR	9
#define CS_DATA_NOBIN	10
#define CS_DATA_NOVBIN	11
#define CS_DATA_NOMNY8	12
#define CS_DATA_NOMNY4	13
#define CS_DATA_NODATE8	14
#define CS_DATA_NODATE4	15
#define CS_DATA_NOFLT4	16
#define CS_DATA_NOFLT8	17
#define CS_DATA_NONUM	18
#define CS_DATA_NOTEXT	19
#define CS_DATA_NOIMAGE	20
#define CS_DATA_NODEC	21
#define CS_DATA_NOLCHAR	22
#define CS_DATA_NOLBIN	23
#define CS_DATA_NOINTN	24
#define CS_DATA_NODATETIMEN	25
#define CS_DATA_NOMONEYN	26
#define CS_CON_NOOOB	27
#define CS_CON_NOINBAND	28
#define CS_PROTO_NOTEXT	29
#define CS_PROTO_NOBULK	30
#define CS_DATA_NOSENSITIVITY	31
#define CS_DATA_NOBOUNDARY	32
#define CS_DATA_NOTDSDEBUG	33
#define CS_RES_NOSTRIPBLANKS	34
#define CS_DATA_NOINT8	35

/* Properties */
enum {
	CS_USERNAME = 1,
/* These defines looks weird but programs can test support for defines, 
   compiler can check enum and there are no define side effecs */
#define CS_USERNAME CS_USERNAME
	CS_PASSWORD,
#define CS_PASSWORD CS_PASSWORD
	CS_APPNAME,
#define CS_APPNAME CS_APPNAME
	CS_HOSTNAME,
#define CS_HOSTNAME CS_HOSTNAME
	CS_PACKETSIZE,
#define CS_PACKETSIZE CS_PACKETSIZE
	CS_SEC_ENCRYPTION,
#define CS_SEC_ENCRYPTION CS_SEC_ENCRYPTION
	CS_LOC_PROP,
#define CS_LOC_PROP CS_LOC_PROP
	CS_SEC_CHALLENGE,
#define CS_SEC_CHALLENGE CS_SEC_CHALLENGE
	CS_SEC_NEGOTIATE,
#define CS_SEC_NEGOTIATE CS_SEC_NEGOTIATE
	CS_TDS_VERSION,
#define CS_TDS_VERSION CS_TDS_VERSION
	CS_NETIO,
#define CS_NETIO CS_NETIO
	CS_IFILE,
#define CS_IFILE CS_IFILE
	CS_USERDATA,
#define CS_USERDATA CS_USERDATA
	CS_SEC_APPDEFINED,
#define CS_SEC_APPDEFINED CS_SEC_APPDEFINED
	CS_CHARSETCNV,
#define CS_CHARSETCNV CS_CHARSETCNV
	CS_ANSI_BINDS,
#define CS_ANSI_BINDS CS_ANSI_BINDS
	CS_VER_STRING
#define CS_VER_STRING CS_VER_STRING
};

/* Arbitrary precision math operators */
enum {
	CS_ADD = 1,
	CS_SUB,
	CS_MULT,
	CS_DIV
};

enum {
	CS_TDS_40 = 1,
	CS_TDS_42,
	CS_TDS_46,
	CS_TDS_495,
	CS_TDS_50,
	CS_TDS_70
};

/* bit mask values used by CS_DATAFMT.status */
#define CS_CANBENULL   (1)
#define CS_HIDDEN      (1 << 1)
#define CS_IDENTITY    (1 << 2)
#define CS_KEY         (1 << 3)
#define CS_VERSION_KEY (1 << 4)
#define CS_TIMESTAMP   (1 << 5)
#define CS_UPDATABLE   (1 << 6)
#define CS_UPDATECOL   (1 << 7)
#define CS_RETURN      (1 << 8)

/* DBD::Sybase compares indicator to CS_NULLDATA so this is -1
** (the documentation states -1) */
#define CS_NULLDATA	(-1)

/* CS_CON_STATUS read-only property bit mask values */
#define CS_CONSTAT_CONNECTED	0x01
#define CS_CONSTAT_DEAD 	0x02

/* options accepted by ct_options() */
#define CS_OPT_ANSINULL		1
#define CS_OPT_ANSIPERM		2
#define CS_OPT_ARITHABORT	3
#define CS_OPT_ARITHIGNORE	4
#define CS_OPT_AUTHOFF		5
#define CS_OPT_AUTHON		6
#define CS_OPT_CHAINXACTS	7
#define CS_OPT_CURCLOSEONXACT	8
#define CS_OPT_CURREAD		9
#define CS_OPT_CURWRITE		10
#define CS_OPT_DATEFIRST	11
#define CS_OPT_DATEFORMAT	12
#define CS_OPT_FIPSFLAG		13
#define CS_OPT_FORCEPLAN	14
#define CS_OPT_FORMATONLY	15
#define CS_OPT_GETDATA		16
#define CS_OPT_IDENTITYOFF	17
#define CS_OPT_IDENTITYON	18
#define CS_OPT_ISOLATION	19
#define CS_OPT_NOCOUNT		20
#define CS_OPT_NOEXEC		21
#define CS_OPT_PARSEONLY	22
#define CS_OPT_QUOTED_IDENT	23
#define CS_OPT_RESTREES		24
#define CS_OPT_ROWCOUNT		25
#define CS_OPT_SHOWPLAN		26
#define CS_OPT_STATS_IO		27
#define CS_OPT_STATS_TIME	28
#define CS_OPT_STR_RTRUNC	29
#define CS_OPT_TEXTSIZE		30
#define CS_OPT_TRUNCIGNORE	31

/* options accepted by ct_command() */
enum ct_command_options {
	CS_MORE, 
	CS_END, 
	CS_UNUSED, 
	CS_RECOMPILE, 
	CS_NO_RECOMPILE, 
	CS_COLUMN_DATA, 
	CS_BULK_DATA, 
	CS_BULK_INIT, 
	CS_BULK_CONT
};


/* bind formats, should be mapped to TDS types 
 * can be a combination of bit */
enum {
	CS_FMT_UNUSED = 0,
#define CS_FMT_UNUSED CS_FMT_UNUSED
	CS_FMT_NULLTERM = 1,
#define CS_FMT_NULLTERM CS_FMT_NULLTERM
	CS_FMT_PADNULL = 2,
#define CS_FMT_PADBLANK CS_FMT_PADBLANK
	CS_FMT_PADBLANK = 4
#define CS_FMT_PADNULL CS_FMT_PADNULL
};

/* callbacks */
#define CS_COMPLETION_CB	1
#define CS_SERVERMSG_CB		2
#define CS_CLIENTMSG_CB		3
#define CS_NOTIF_CB		4
#define CS_ENCRYPT_CB		5
#define CS_CHALLENGE_CB		6
#define CS_DS_LOOKUP_CB		7
#define CS_SECSESSION_CB	8
#define CS_SIGNAL_CB		100
#define CS_MESSAGE_CB		9119

/* string types */
#define CS_NULLTERM	TDS_NULLTERM
#define CS_WILDCARD	-99
#define CS_NO_LIMIT	-9999
#define CS_UNUSED	-99999

/* other */
#define CS_CLEAR	3
#define CS_SET		4
#define CS_LANG_CMD	7
#define CS_ROW_FAIL	9
#define CS_END_DATA	10
#define CS_END_ITEM 11
#define CS_CMD_SUCCEED	TDS_CMD_SUCCEED
#define CS_CMD_FAIL	TDS_CMD_FAIL
#define CS_CMD_DONE	TDS_CMD_DONE
#define CS_END_RESULTS	15
#define CS_VERSION_100	16
#define CS_FORCE_EXIT	17
#define CS_GET		25
#define CS_CON_STATUS 26
#define CS_FORCE_CLOSE 27
#define CS_SYNC_IO	29
#define CS_LC_ALL	37
#define CS_SYB_LANG	38
#define CS_SYB_CHARSET	39
#define CS_SV_COMM_FAIL	41
#define CS_BULK_LOGIN	42
#define BLK_VERSION_100 CS_VERSION_100
#define CS_BLK_IN	43
#define CS_BLK_OUT	44
#define CS_BLK_BATCH	45
#define CS_BLK_ALL	46
#define CS_BLK_CANCEL	47
#define CS_CANCEL_ALL	48
#define CS_NUMDATA	49
#define CS_CANCEL_ATTN	50
#define CS_PARENT_HANDLE	51
#define CS_COMP_ID	52
#define CS_BYLIST_LEN	53
#define CS_COMP_BYLIST	54
#define CS_COMP_OP	55
#define CS_COMP_COLID	56
#define CS_NO_COUNT	57
#define CS_ROW_COUNT	59
#define CS_OP_SUM	60
#define CS_OP_AVG	61
#define CS_OP_MIN	62
#define CS_OP_MAX	63
#define CS_OP_COUNT	64
#define CS_CANCEL_CURRENT	67
#define CS_CAPREQUEST	73
#define CS_EED_CMD	77
#define CS_LOGIN_TIMEOUT	78
#define CS_CAP_REQUEST	79
#define CS_DESCRIBE_INPUT	80
#define CS_PREPARE	81
#define CS_EXECUTE	82
#define CS_DEALLOC	83
#define CS_CAP_RESPONSE	84
#define CS_RPC_CMD	85
/* need correct value for CS_SEND_BULK_CMD  */
#define CS_SEND_BULK_CMD 0xFFFF
#define CS_INPUTVALUE	86
#define CS_GOODDATA	87
/* define CS_RETURN	88 */
#define CS_CMD_NUMBER	89
#define CS_BROWSE_INFO	90
#define CS_NUMORDERCOLS	91
#define CS_NUM_COMPUTES	92
#define CS_NODATA	96
#define CS_DESCIN	98
#define CS_DESCOUT	99
/* define CS_UPDATECOL	100 */
#define CS_NODEFAULT	102
#define CS_FMT_JUSTIFY_RT	106
#define CS_TRANS_STATE	107
#define CS_TRAN_IN_PROGRESS	108
#define CS_TRAN_COMPLETED	109
#define CS_TRAN_STMT_FAIL	110
#define CS_TRAN_FAIL	111
#define CS_TRAN_UNDEFINED	112
#define CS_SV_RETRY_FAIL	114
#define CS_TIMEOUT	115
#define CS_CANCELED 	116
#define CS_NO_RECOMPILE	117
#define CS_COLUMN_DATA	118
#define CS_SEND_DATA_CMD	119
#define CS_SUPPORTED 120
#define CS_EXPOSE_FMTS 121
#define CS_VERSION	9114
#define CS_EXTRA_INF	9121

/* result_types */
#define CS_COMPUTE_RESULT	TDS_COMPUTE_RESULT
#define CS_CURSOR_RESULT	4041
#define CS_PARAM_RESULT		TDS_PARAM_RESULT
#define CS_ROW_RESULT		TDS_ROW_RESULT
#define CS_STATUS_RESULT	TDS_STATUS_RESULT
#define CS_COMPUTEFMT_RESULT	TDS_COMPUTEFMT_RESULT
#define CS_ROWFMT_RESULT	TDS_ROWFMT_RESULT
#define CS_MSG_RESULT		TDS_MSG_RESULT
#define CS_DESCRIBE_RESULT	TDS_DESCRIBE_RESULT

/* bind types */
#define CS_CHAR_TYPE	1
#define CS_INT_TYPE	2
#define CS_SMALLINT_TYPE	3
#define CS_TINYINT_TYPE	4
#define CS_MONEY_TYPE	5
#define CS_DATETIME_TYPE	6
#define CS_NUMERIC_TYPE	7
#define CS_DECIMAL_TYPE	8
#define CS_DATETIME4_TYPE	9
#define CS_MONEY4_TYPE	10
#define CS_IMAGE_TYPE	11
#define CS_BINARY_TYPE	12
#define CS_BIT_TYPE	13
#define CS_REAL_TYPE	14
#define CS_FLOAT_TYPE	15
#define CS_TEXT_TYPE	16
#define CS_VARCHAR_TYPE	17
#define CS_VARBINARY_TYPE	18
#define CS_LONGCHAR_TYPE	19
#define CS_LONGBINARY_TYPE	20
#define CS_LONG_TYPE	21
#define CS_ILLEGAL_TYPE	22
#define CS_SENSITIVITY_TYPE	23
#define CS_BOUNDARY_TYPE	24
#define CS_VOID_TYPE	25
#define CS_USHORT_TYPE	26
#define CS_UNIQUE_TYPE	27

/* cs_dt_info type values */
enum {
	CS_MONTH = 1,
#define CS_MONTH CS_MONTH
	CS_SHORTMONTH,
#define CS_SHORTMONTH CS_SHORTMONTH
	CS_DAYNAME,
#define CS_DAYNAME CS_DAYNAME
	CS_DATEORDER,
#define CS_DATEORDER CS_DATEORDER
	CS_12HOUR,
#define CS_12HOUR CS_12HOUR
	CS_DT_CONVFMT
#define CS_DT_CONVFMT CS_DT_CONVFMT
};

/* DT_CONVFMT types */
enum {
	CS_DATES_HMS = 1,
#define CS_DATES_HMS CS_DATES_HMS
	CS_DATES_SHORT,
#define CS_DATES_SHORT CS_DATES_SHORT
	CS_DATES_LONG,
#define CS_DATES_LONG CS_DATES_LONG
	CS_DATES_MDY1,
#define CS_DATES_MDY1 CS_DATES_MDY1
	CS_DATES_MYD1,
#define CS_DATES_MYD1 CS_DATES_MYD1
	CS_DATES_DMY1,
#define CS_DATES_DMY1 CS_DATES_DMY1
	CS_DATES_DYM1,
#define CS_DATES_DYM1 CS_DATES_DYM1
	CS_DATES_YDM1,
#define CS_DATES_YDM1 CS_DATES_YDM1
	CS_DATES_YMD2,
#define CS_DATES_YMD2 CS_DATES_YMD2
	CS_DATES_MDY1_YYYY,
#define CS_DATES_MDY1_YYYY CS_DATES_MDY1_YYYY
	CS_DATES_DMY1_YYYY,
#define CS_DATES_DMY1_YYYY CS_DATES_DMY1_YYYY
	CS_DATES_YMD2_YYYY,
#define CS_DATES_YMD2_YYYY CS_DATES_YMD2_YYYY
	CS_DATES_DMY2,
#define CS_DATES_DMY2 CS_DATES_DMY2
	CS_DATES_YMD1,
#define CS_DATES_YMD1 CS_DATES_YMD1
	CS_DATES_DMY2_YYYY,
#define CS_DATES_DMY2_YYYY CS_DATES_DMY2_YYYY
	CS_DATES_YMD1_YYYY,
#define CS_DATES_YMD1_YYYY CS_DATES_YMD1_YYYY
	CS_DATES_DMY4,
#define CS_DATES_DMY4 CS_DATES_DMY4
	CS_DATES_DMY4_YYYY,
#define CS_DATES_DMY4_YYYY CS_DATES_DMY4_YYYY
	CS_DATES_MDY2,
#define CS_DATES_MDY2 CS_DATES_MDY2
	CS_DATES_MDY2_YYYY,
#define CS_DATES_MDY2_YYYY CS_DATES_MDY2_YYYY
	CS_DATES_DMY3,
#define CS_DATES_DMY3 CS_DATES_DMY3
	CS_DATES_MDY3,
#define CS_DATES_MDY3 CS_DATES_MDY3
	CS_DATES_DMY3_YYYY,
#define CS_DATES_DMY3_YYYY CS_DATES_DMY3_YYYY
	CS_DATES_MDY3_YYYY,
#define CS_DATES_MDY3_YYYY CS_DATES_MDY3_YYYY
	CS_DATES_YMD3,
#define CS_DATES_YMD3 CS_DATES_YMD3
	CS_DATES_YMD3_YYYY
#define CS_DATES_YMD3_YYYY CS_DATES_YMD3_YYYY
};

typedef CS_RETCODE (*CS_CONV_FUNC)(CS_CONTEXT *context, CS_DATAFMT *srcfmt, CS_VOID *src, CS_DATAFMT *detsfmt, CS_VOID *dest, CS_INT *destlen);

typedef struct _cs_objname {
	CS_BOOL thinkexists;
	CS_INT object_type;
	CS_CHAR last_name[CS_MAX_NAME];
	CS_INT lnlen;
	CS_CHAR first_name[CS_MAX_NAME];
	CS_INT fnlen;
	CS_VOID *scope;
	CS_INT scopelen;
	CS_VOID *thread;
	CS_INT threadlen;
} CS_OBJNAME;

typedef struct _cs_objdata {
	CS_BOOL actuallyexists;
	CS_CONNECTION *connection;
	CS_COMMAND *command;
	CS_VOID *buffer;
	CS_INT buflen;
} CS_OBJDATA;

/* Eventually, these should be in terms of TDS values */
enum {
        CS_OPT_SUNDAY,
        CS_OPT_MONDAY,
        CS_OPT_TUESDAY,
        CS_OPT_WEDNESDAY,
        CS_OPT_THURSDAY,
        CS_OPT_FRIDAY,
        CS_OPT_SATURDAY
};
enum {
	CS_OPT_FMTMDY,
	CS_OPT_FMTDMY,
	CS_OPT_FMTYMD,
	CS_OPT_FMTYDM,
	CS_OPT_FMTMYD,
	CS_OPT_FMTDYM
};
enum {
	CS_OPT_LEVEL0,
	CS_OPT_LEVEL1,
	CS_OPT_LEVEL3
};

/* */
#define CS_FALSE	0
#define CS_TRUE	1

#define SRV_PROC	CS_VOID
#define CS_BLK_ROW	CS_VOID

/* constants required for ct_diag (not jet implemented) */
#define CS_INIT 36
#define CS_STATUS 37
#define CS_MSGLIMIT 38
#define CS_CLIENTMSG_TYPE 4700
#define CS_SERVERMSG_TYPE 4701
#define CS_ALLMSG_TYPE 4702

CS_RETCODE cs_convert(CS_CONTEXT *ctx, CS_DATAFMT *srcfmt, CS_VOID *srcdata, CS_DATAFMT *destfmt, CS_VOID *destdata, CS_INT *resultlen);
CS_RETCODE cs_ctx_alloc(CS_INT version, CS_CONTEXT **ctx);
CS_RETCODE cs_ctx_global(CS_INT version, CS_CONTEXT **ctx);
CS_RETCODE cs_ctx_drop(CS_CONTEXT *ctx);
CS_RETCODE cs_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE cs_strbuild(CS_CONTEXT *ctx, CS_CHAR *buffer, CS_INT buflen, CS_INT *resultlen, CS_CHAR *text, CS_INT textlen, CS_CHAR *formats, CS_INT formatlen, ...);
CS_RETCODE cs_dt_crack(CS_CONTEXT *ctx, CS_INT datetype, CS_VOID *dateval, CS_DATEREC *daterec);
CS_RETCODE cs_loc_alloc(CS_CONTEXT *ctx, CS_LOCALE **locptr);
CS_RETCODE cs_loc_drop(CS_CONTEXT *ctx, CS_LOCALE *locale);
CS_RETCODE cs_locale(CS_CONTEXT *ctx, CS_INT action, CS_LOCALE *locale, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE cs_dt_info(CS_CONTEXT *ctx, CS_INT action, CS_LOCALE *locale, CS_INT type, CS_INT item, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);

CS_RETCODE cs_calc(CS_CONTEXT *ctx, CS_INT op, CS_INT datatype, CS_VOID *var1, CS_VOID *var2, CS_VOID *dest);
CS_RETCODE cs_cmp(CS_CONTEXT *ctx, CS_INT datatype, CS_VOID *var1, CS_VOID *var2, CS_INT *result);
CS_RETCODE cs_conv_mult(CS_CONTEXT *ctx, CS_LOCALE *srcloc, CS_LOCALE *destloc, CS_INT *conv_multiplier);
CS_RETCODE cs_diag(CS_CONTEXT *ctx, CS_INT operation, CS_INT type, CS_INT idx, CS_VOID *buffer);
CS_RETCODE cs_manage_convert(CS_CONTEXT *ctx, CS_INT action, CS_INT srctype, CS_CHAR *srcname, CS_INT srcnamelen, CS_INT desttype, CS_CHAR *destname, CS_INT destnamelen, CS_INT *conv_multiplier, CS_CONV_FUNC *func);
CS_RETCODE cs_objects(CS_CONTEXT *ctx, CS_INT action, CS_OBJNAME *objname, CS_OBJDATA *objdata);
CS_RETCODE cs_set_convert(CS_CONTEXT *ctx, CS_INT action, CS_INT srctype, CS_INT desttype, CS_CONV_FUNC *func);
CS_RETCODE cs_setnull(CS_CONTEXT *ctx, CS_DATAFMT *datafmt, CS_VOID *buffer, CS_INT buflen);
CS_RETCODE cs_strcmp(CS_CONTEXT *ctx, CS_LOCALE *locale, CS_INT type, CS_CHAR *str1, CS_INT len1, CS_CHAR *str2, CS_INT len2, CS_INT *result);
CS_RETCODE cs_time(CS_CONTEXT *ctx, CS_LOCALE *locale, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen, CS_DATEREC *daterec);
CS_RETCODE cs_will_convert(CS_CONTEXT *ctx, CS_INT srctype, CS_INT desttype, CS_BOOL *result);

#ifdef __cplusplus
}
#endif 

#endif

