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
         "$Id: cspublic.h,v 1.12 2002-09-26 21:10:16 castellano Exp $";
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

#define CS_FAIL	   TDS_FAIL
#define CS_SUCCEED TDS_SUCCEED
#define CS_SIZEOF(x) sizeof(x)

#define CS_LAYER(x)    (((x) >> 24) & 0xFF)
#define CS_ORIGIN(x)   (((x) >> 16) & 0xFF)
#define CS_SEVERITY(x) (((x) >>  8) & 0xFF)
#define CS_NUMBER(x)   ((x) & 0xFF)

/* forward declarations */
typedef struct cs_context CS_CONTEXT;
typedef struct cs_clientmsg CS_CLIENTMSG;
typedef struct cs_connection CS_CONNECTION;
typedef struct cs_servermsg CS_SERVERMSG;

struct cs_context
{
	CS_INT date_convert_fmt;
	CS_RETCODE (*_cslibmsg_cb)(CS_CONTEXT *, CS_CLIENTMSG *);
	CS_RETCODE (*_clientmsg_cb)(CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *);
	CS_RETCODE (*_servermsg_cb)(CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *);
	TDSCONTEXT *tds_ctx;
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
	void *tds_login;
	TDSSOCKET *tds_socket;
	CS_RETCODE (*_clientmsg_cb)(CS_CONTEXT *, CS_CONNECTION *, CS_CLIENTMSG *);
	CS_RETCODE (*_servermsg_cb)(CS_CONTEXT *, CS_CONNECTION *, CS_SERVERMSG *);
	void *userdata;
	int userdata_len;
	CS_LOCALE *locale;
};

typedef struct cs_command
{
	CS_CHAR *query;
	int cmd_done;
	CS_CONNECTION *con;
	void *userdata;
	int userdata_len;
	short empty_res_hack;
	short dynamic_cmd;
	char  *dyn_id; 
} CS_COMMAND;

#define CS_MAX_MSG 1024
#define CS_MAX_NAME 132
#define CS_MAX_PREC 77  /* used by php */
#define CS_OBJ_NAME 132 /* ? */
#define CS_TP_SIZE  16  /* ? */
#define CS_TS_SIZE  16  /* ? */
#define CS_SQLSTATE_SIZE 8


#define CS_SRC_VALUE   -999

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

/* CS_CAP_REQUEST values */
#define CS_CON_INBAND	1
#define CS_CON_OOB	2
#define CS_CSR_ABS	3
#define CS_CSR_FIRST	4
#define CS_CSR_LAST	5
#define CS_CSR_MULTI	6
#define CS_CSR_PREV	7
#define CS_CSR_REL	8
#define CS_DATA_BIN	9
#define CS_DATA_VBIN	10
#define CS_DATA_LBIN	11
#define CS_DATA_BIT	12
#define CS_DATA_BITN	13
#define CS_DATA_BOUNDARY	14
#define CS_DATA_CHAR	15
#define CS_DATA_VCHAR	16
#define CS_DATA_LCHAR	17
#define CS_DATA_DATE4	18
#define CS_DATA_DATE8	19
#define CS_DATA_DATETIMEN	20
#define CS_DATA_DEC	21
#define CS_DATA_FLT4	22
#define CS_DATA_FLT8	23
#define CS_DATA_FLTN	24
#define CS_DATA_IMAGE	25
#define CS_DATA_INT1	26
#define CS_DATA_INT2	27
#define CS_DATA_INT4	28
#define CS_DATA_INTN	29
#define CS_DATA_MNY4	30
#define CS_DATA_MNY8	31
#define CS_DATA_MONEYN	32
#define CS_DATA_NUM	33
#define CS_DATA_SENSITIVITY	34
#define CS_DATA_TEXT	35
#define CS_OPTION_GET	36
#define CS_PROTO_BULK	37
#define CS_PROTO_DYNAMIC	38
#define CS_PROTO_DYNPROC	39
#define CS_REQ_BCP	40
#define CS_REQ_CURSOR	41
#define CS_REQ_DYN	42
#define CS_REQ_LANG	43
#define CS_REQ_MSG	44
#define CS_REQ_MSTMT	45
#define CS_REQ_NOTIF	46
#define CS_REQ_PARAM	47
#define CS_REQ_URGNOTIF	48
#define CS_REQ_RPC	49
#define CS_DATA_INT8	50
#define CS_DATA_VOID	51
#define CS_CON_LOGICAL	52
#define CS_PROTO_TEXT	53

/* CS_CAP_RESPONSE values */
#define CS_DATA_NOBOUNDARY	1
#define CS_DATA_NOTDSDEBUG	2
#define CS_RES_NOSTRIPBLANKS	3
#define CS_DATA_NOINT8	4
#define CS_DATA_NOINTN	5
#define CS_DATA_NODATETIMEN	6
#define CS_DATA_NOMONEYN	7
#define CS_CON_NOOOB	8
#define CS_CON_NOINBAND	9
#define CS_PROTO_NOTEXT	10
#define CS_PROTO_NOBULK	11
#define CS_DATA_NOSENSITIVITY	12
#define CS_DATA_NOFLT4	13
#define CS_DATA_NOFLT8	14
#define CS_DATA_NONUM	15
#define CS_DATA_NOTEXT	16
#define CS_DATA_NOIMAGE	17
#define CS_DATA_NODEC	18
#define CS_DATA_NOLCHAR	19
#define CS_DATA_NOLBIN	20
#define CS_DATA_NOCHAR	21
#define CS_DATA_NOVCHAR	22
#define CS_DATA_NOBIN	23
#define CS_DATA_NOVBIN	24
#define CS_DATA_NOMNY8	25
#define CS_DATA_NOMNY4	26
#define CS_DATA_NODATE8	27
#define CS_DATA_NODATE4	28
#define CS_RES_NOMSG	29
#define CS_RES_NOEED	30
#define CS_RES_NOPARAM	31
#define CS_DATA_NOINT1	32
#define CS_DATA_NOINT2	33
#define CS_DATA_NOINT4	34
#define CS_DATA_NOBIT	35

/* Properties */
enum {
	CS_USERNAME = 1,
	CS_PASSWORD,
	CS_APPNAME,
	CS_HOSTNAME,
	CS_PACKETSIZE,
	CS_SEC_ENCRYPTION,
	CS_LOC_PROP,
	CS_SEC_CHALLENGE,
	CS_SEC_NEGOTIATE,
	CS_TDS_VERSION,
	CS_NETIO,
	CS_IFILE,
	CS_USERDATA,
	CS_SEC_APPDEFINED,
	CS_CHARSETCNV,
	CS_ANSI_BINDS,
	CS_VER_STRING
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

/* fields used by CS_DATAFMT.status */
#define CS_CANBENULL   (1)
#define CS_HIDDEN      (1 << 1)
#define CS_IDENTITY    (1 << 2)
#define CS_KEY         (1 << 3)
#define CS_VERSION_KEY (1 << 4)
#define CS_TIMESTAMP   (1 << 5)
#define CS_UPDATABLE   (1 << 6)

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

/* bind formats, should be mapped to TDS types */
enum {
	CS_FMT_UNUSED = 0,
	CS_FMT_NULLTERM,
	CS_FMT_PADBLANK,
	CS_FMT_PADNULL
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
#define CS_SET		4
#define CS_LANG_CMD	7
#define CS_ROW_FAIL	9
#define CS_END_DATA	10
#define CS_CMD_SUCCEED	12
#define CS_CMD_FAIL	13
#define CS_CMD_DONE	14
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
#define CS_INPUTVALUE	86
#define CS_GOODDATA	87
#define CS_RETURN	88
#define CS_CMD_NUMBER	89
#define CS_BROWSE_INFO	90
#define CS_NUMORDERCOLS	91
#define CS_NUM_COMPUTES	92
#define CS_NODATA	96
#define CS_DESCIN	98
#define CS_DESCOUT	99
#define CS_UPDATECOL	100
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
#define CS_VERSION	9114
#define CS_EXTRA_INF	9121

/* result_types */
#define CS_COMPUTE_RESULT	1
#define CS_CURSOR_RESULT	2
#define CS_PARAM_RESULT		3
#define CS_ROW_RESULT		4
#define CS_STATUS_RESULT	5
#define CS_COMPUTEFMT_RESULT	6
#define CS_ROWFMT_RESULT	7
#define CS_MSG_RESULT		8
#define CS_DESCRIBE_RESULT	9

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
	CS_SHORTMONTH,
	CS_DAYNAME,
	CS_DATEORDER,
	CS_12HOUR,
	CS_DT_CONVFMT
};

/* DT_CONVFMT types */
enum {
	CS_DATES_HMS = 1,
	CS_DATES_SHORT,
	CS_DATES_LONG,
	CS_DATES_MDY1,
	CS_DATES_MYD1,
	CS_DATES_DMY1,
	CS_DATES_DYM1,
	CS_DATES_YDM1,
	CS_DATES_YMD2,
	CS_DATES_MDY1_YYYY,
	CS_DATES_DMY1_YYYY,
	CS_DATES_YMD2_YYYY,
	CS_DATES_DMY2,
	CS_DATES_YMD1,
	CS_DATES_DMY2_YYYY,
	CS_DATES_YMD1_YYYY,
	CS_DATES_DMY4,
	CS_DATES_DMY4_YYYY,
	CS_DATES_MDY2,
	CS_DATES_MDY2_YYYY,
	CS_DATES_DMY3,
	CS_DATES_MDY3,
	CS_DATES_DMY3_YYYY,
	CS_DATES_MDY3_YYYY,
	CS_DATES_YMD3,
	CS_DATES_YMD3_YYYY
};

/* */
#define CS_FALSE	0
#define CS_TRUE	1

CS_RETCODE cs_convert(CS_CONTEXT *ctx, CS_DATAFMT *srcfmt, CS_VOID *srcdata, CS_DATAFMT *destfmt, CS_VOID *destdata, CS_INT *resultlen);
CS_RETCODE cs_ctx_alloc(CS_INT version, CS_CONTEXT **ctx);
CS_RETCODE cs_ctx_drop(CS_CONTEXT *ctx);
CS_RETCODE cs_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE cs_strbuild(CS_CONTEXT *ctx, CS_CHAR *buffer, CS_INT buflen, CS_INT *resultlen, CS_CHAR *text, CS_INT textlen, CS_CHAR *formats, CS_INT formatlen, ...);

#ifdef __cplusplus
}
#endif 

#endif

