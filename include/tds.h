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
	"$Id: tds.h,v 1.61 2002-11-29 11:35:46 freddy77 Exp $";
static void *no_unused_tds_h_warn[] = {
	rcsid_tds_h,
	no_unused_tds_h_warn};

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "tdsver.h"
#include "tds_sysdep_public.h"
#ifdef _FREETDS_LIBRARY_SOURCE
#include "tds_sysdep_private.h"
#endif /* _FREETDS_LIBRARY_SOURCE */

#ifdef __cplusplus
extern "C" {
#endif 

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

#define TDS_SUCCEED          1
#define TDS_FAIL             0
#define TDS_NO_MORE_RESULTS  2
#define TDS_REG_ROW          -1
#define TDS_NO_MORE_ROWS     -2
#define TDS_COMP_ROW         -3

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
	TDS_DONE_MORE_RESULTS = 1,
	TDS_DONE_ERROR = 2,
	TDS_DONE_COUNT = 16,
	TDS_DONE_CANCELLED = 32
};


/*
** TDS_ERROR indicates a successful processing, but an TDS_ERR_TOKEN or 
** TDS_EED_TOKEN error was encountered, whereas TDS_FAIL indicates an
** unrecoverable failure.
*/
#define TDS_ERROR            3  
#define TDS_DONT_RETURN      42

#define TDS5_DYN_TOKEN      231  /* 0xE7    TDS 5.0 only              */
#define TDS5_PARAMFMT_TOKEN 236  /* 0xEC    TDS 5.0 only              */
#define TDS5_PARAMS_TOKEN   215  /* 0xD7    TDS 5.0 only              */
#define TDS_LANG_TOKEN       33  /* 0x21    TDS 5.0 only              */
#define TDS_CLOSE_TOKEN     113  /* 0x71    TDS 5.0 only? ct_close()  */
#define TDS_RET_STAT_TOKEN  121  /* 0x79                              */
#define TDS_PROCID_TOKEN    124  /* 0x7C    TDS 4.2 only - TDS_PROCID */
#define TDS7_RESULT_TOKEN   129  /* 0x81    TDS 7.0 only              */
#define TDS7_COMPUTE_RESULT_TOKEN   136  /* 0x88    TDS 7.0 only              */
#define TDS_COL_NAME_TOKEN  160  /* 0xA0    TDS 4.2 only              */
#define TDS_COL_INFO_TOKEN  161  /* 0xA1    TDS 4.2 only - TDS_COLFMT */
/*#define  TDS_TABNAME   164 */
/*#define  TDS_COL_INFO   165 */
#define TDS_COMPUTE_NAMES_TOKEN   167  /* 0xA7                        */
#define TDS_COMPUTE_RESULT_TOKEN  168  /* 0xA8                        */
#define TDS_ORDER_BY_TOKEN  169  /* 0xA9    TDS_ORDER                 */
#define TDS_ERR_TOKEN       170  /* 0xAA                              */
#define TDS_MSG_TOKEN       171  /* 0xAB                              */
#define TDS_PARAM_TOKEN     172  /* 0xAC    RETURNVALUE?              */
#define TDS_LOGIN_ACK_TOKEN 173  /* 0xAD                              */
#define TDS_CONTROL_TOKEN   174  /* 0xAE    TDS_CONTROL               */
#define TDS_ROW_TOKEN       209  /* 0xD1                              */
#define TDS_CMP_ROW_TOKEN   211  /* 0xD3                              */
#define TDS_CAP_TOKEN       226  /* 0xE2                              */
#define TDS_ENV_CHG_TOKEN   227  /* 0xE3                              */
#define TDS_EED_TOKEN       229  /* 0xE5                              */
#define TDS_AUTH_TOKEN      237  /* 0xED                              */
#define TDS_RESULT_TOKEN    238  /* 0xEE                              */
#define TDS_DONE_TOKEN      253  /* 0xFD    TDS_DONE                  */
#define TDS_DONEPROC_TOKEN  254  /* 0xFE    TDS_DONEPROC              */
#define TDS_DONEINPROC_TOKEN 255  /* 0xFF    TDS_DONEINPROC            */

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

/* string types */
#define TDS_NULLTERM -9

/* 
<rant> Sybase does an awful job of this stuff, non null ints of size 1 2 
and 4 have there own codes but nullable ints are lumped into INTN
sheesh! </rant>
*/
#define SYBCHAR      47   /* 0x2F */
#define SYBVARCHAR   39   /* 0x27 */
#define SYBINTN      38   /* 0x26 */
#define SYBINT1      48   /* 0x30 */
#define SYBINT2      52   /* 0x34 */
#define SYBINT4      56   /* 0x38 */
#define SYBINT8     127   /* 0x7F */
#define SYBFLT8      62   /* 0x3E */
#define SYBDATETIME  61   /* 0x3D */
#define SYBBIT       50   /* 0x32 */
#define SYBTEXT      35   /* 0x23 */
#define SYBNTEXT     99   /* 0x63 */
#define SYBIMAGE     34   /* 0x22 */
#define SYBMONEY4    122  /* 0x7A */
#define SYBMONEY     60   /* 0x3C */
#define SYBDATETIME4 58   /* 0x3A */
#define SYBREAL      59   /* 0x3B */
#define SYBBINARY    45   /* 0x2D */
#define SYBVOID      31   /* 0x1F */
#define SYBVARBINARY 37   /* 0x25 */
#define SYBNVARCHAR  103  /* 0x67 */
#define SYBBITN      104  /* 0x68 */
#define SYBNUMERIC   108  /* 0x6C */
#define SYBDECIMAL   106  /* 0x6A */
#define SYBFLTN      109  /* 0x6D */
#define SYBMONEYN    110  /* 0x6E */
#define SYBDATETIMN  111  /* 0x6F */
#define XSYBCHAR     175  /* 0xAF */
#define XSYBVARCHAR  167  /* 0xA7 */
#define XSYBNVARCHAR 231  /* 0xE7 */
#define XSYBNCHAR    239  /* 0xEF */
#define XSYBVARBINARY 165  /* 0xA5 */
#define XSYBBINARY    173  /* 0xAD */

#define SYBUNIQUE    36    /* 0x24 */
#define SYBVARIANT   0x62

#define SYBAOPCNT  0x4b
#define SYBAOPCNTU 0x4c
#define SYBAOPSUM  0x4d
#define SYBAOPSUMU 0x4e
#define SYBAOPAVG  0x4f
#define SYBAOPAVGU 0x50
#define SYBAOPMIN  0x51
#define SYBAOPMAX  0x52

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

#define is_msg_token(x) (x==TDS_MSG_TOKEN    || \
			x==TDS_ERR_TOKEN    || \
			x==TDS_EED_TOKEN)

#define is_result_token(x) (x==TDS_RESULT_TOKEN    || \
			x==TDS7_RESULT_TOKEN    || \
			x==TDS_COL_INFO_TOKEN    || \
			x==TDS_COL_NAME_TOKEN)

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
#define is_large_type(x) (x>128)
#define is_numeric_type(x) (x==SYBNUMERIC || x==SYBDECIMAL)
#define is_unicode(x) (x==XSYBNVARCHAR || x==XSYBNCHAR || x==SYBNTEXT)
#define is_collate_type(x) (x==XSYBVARCHAR || x==XSYBCHAR || x==SYBTEXT || x == XSYBNVARCHAR || x==SYBNTEXT)

#define TDS_MAX_CAPABILITY	18
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

/* TODO do a best check for alignment than this */
typedef union { void *p; int i; } tds_align_struct;
#define TDS_ALIGN_SIZE sizeof(tds_align_struct)

#define TDS_MAX_LOGIN_STR_SZ 30
typedef struct tds_login {
	char *server_name;
	int port;
	TDS_TINYINT  major_version; /* TDS version */
	TDS_TINYINT  minor_version; /* TDS version */
	int block_size; 
	char *language; /* ie us-english */
	char *char_set; /*  ie iso_1 */
	TDS_INT connect_timeout;
	char *host_name;
	char *app_name;
	char *user_name;
	char *password;
	/* Ct-Library, DB-Library,  TDS-Library or ODBC */
	char *library;
	TDS_TINYINT bulk_copy; 
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted; 

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func)(long lHint);
	long longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
} TDSLOGIN;

typedef struct tds_connect_info {
	/* first part of structure is the same of login one */
	char *server_name; /**< server name (in freetds.conf) */
	int port;          /**< port of database service */
	TDS_TINYINT major_version;
	TDS_TINYINT minor_version;
	int block_size;
	char *language;
	char *char_set;    /**< charset of server */
	TDS_INT connect_timeout;
	char *host_name;     /**< client hostname */
	char *app_name;
	char *user_name;     /**< account for login */
	char *password;      /**< password of account login */
	char *library;
	TDS_TINYINT bulk_copy;
	TDS_TINYINT suppress_language;
	TDS_TINYINT encrypted;

	TDS_INT query_timeout;
	TDS_INT longquery_timeout;
	void (*longquery_func)(long lHint);
	long longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];


	char *ip_addr;     /**< ip of server */
	char *database;
	char *dump_file;
	char *default_domain;
	char *client_charset;
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

typedef struct tds_loc_info {
        char *language;
        char *char_set;
        char *date_fmt;
} TDSLOCINFO;

/** structure that hold information about blobs (like text or image).
 * current_row contain this structure.
 */
typedef struct tds_blob_info {
	TDS_CHAR *textvalue;
	TDS_CHAR textptr[16];
	TDS_CHAR timestamp[8];
} TDSBLOBINFO;

/** structure for storing data about regular and compute rows */ 
typedef struct tds_column_info {
	/** type of data, this type can be different from wire type because 
	 *conversion can be applied (like Unicode->Single byte characters) */
	TDS_SMALLINT column_type;
	/** type of data, saved from wire */
	TDS_SMALLINT column_type_save;
	TDS_INT column_usertype;
	TDS_SMALLINT column_flags;
	/** maximun size of data. For fixed is the size. */
	TDS_INT column_size;
	/** size of length when reading from wire (0, 1, 2 or 4) */
	TDS_TINYINT column_varint_size;
	/** precision for decimal/numeric */
	TDS_TINYINT column_prec;
	/** scale for decimal/numeric */
	TDS_TINYINT column_scale;
	/** length of column name */
	TDS_TINYINT column_namelen;
	/* FIXME why 256. bigger limit is 128 ucs2 character ....*/
	/** column name */
	TDS_CHAR column_name[256];
	/** offset into row buffer for store data */
	TDS_INT column_offset;
	unsigned int column_nullable:1;
	unsigned int column_writeable:1;
	unsigned int column_identity:1;
	unsigned int column_unicodedata:1;
	unsigned int column_output:1;
	TDS_CHAR    column_collation[5];

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
	TDS_COMPLETED,
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

/*
typedef struct tds_param_info {
	TDS_SMALLINT  num_cols;
	TDSCOLINFO    **columns;
	TDS_INT       row_size;
	int           null_info_size;
	unsigned char *current_row;
} TDSPARAMINFO;
*/

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
	TDSLOCINFO *locale;
	void *parent;
	/* handler */
	int (*msg_handler)(TDSCONTEXT*, TDSSOCKET*, TDSMSGINFO*);
	int (*err_handler)(TDSCONTEXT*, TDSSOCKET*, TDSMSGINFO*);
};

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
	/* info about current query */
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
	void *iconv_info;

	/** config for login stuff. After login this field is NULL */
	TDSCONNECTINFO *connect_info;
	int spid;
	void (*env_chg_func)(TDSSOCKET *tds, int type, char *oldval, char *newval);
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
int tds_put_n(TDSSOCKET *tds, const unsigned char *buf, int n);
int tds_put_string(TDSSOCKET *tds, const char *buf,int len);
int tds_put_int(TDSSOCKET *tds, TDS_INT i);
int tds_put_smallint(TDSSOCKET *tds, TDS_SMALLINT si);
int tds_put_tinyint(TDSSOCKET *tds, TDS_TINYINT ti);
int tds_put_byte(TDSSOCKET *tds, unsigned char c);
TDSRESULTINFO *tds_alloc_results(int num_cols);
TDSCOMPUTEINFO **tds_alloc_compute_results(TDS_INT *num_comp_results, TDSCOMPUTEINFO** ci, int num_cols, int by_cols);
TDSCONTEXT *tds_alloc_context(void);
void tds_free_context(TDSCONTEXT *locale);
TDSSOCKET *tds_alloc_socket(TDSCONTEXT *context, int bufsize);

/* config.c */
typedef void (*TDSCONFPARSE)(const char* option, const char* value, void *param);
int tds_read_conf_section(FILE *in, const char *section, TDSCONFPARSE tds_conf_parse, void *parse_param);
int tds_read_conf_file(TDSCONNECTINFO *connect_info,const char *server);
TDSCONNECTINFO *tds_read_config_info(TDSSOCKET *tds, TDSLOGIN *login, TDSLOCINFO *locale);
void tds_fix_connect(TDSCONNECTINFO *connect_info);
void tds_config_verstr(const char *tdsver, TDSCONNECTINFO *connect_info);
void tds_lookup_host(const char *servername, const char *portname, char *ip, char *port);
int tds_set_interfaces_file_loc(char *interfloc);

TDSLOCINFO *tds_get_locale(void);
void *tds_alloc_row(TDSRESULTINFO *res_info);
void *tds_alloc_compute_row(TDSCOMPUTEINFO *res_info);
char *tds_alloc_get_string(TDSSOCKET *tds, int len);
TDSLOGIN *tds_alloc_login(void);
TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, const char *id);
void tds_free_login(TDSLOGIN *login);
TDSCONNECTINFO *tds_alloc_connect(TDSLOCINFO *locale);
TDSLOCINFO *tds_alloc_locale(void);
void tds_free_locale(TDSLOCINFO *locale);
int tds_connect(TDSSOCKET *tds, TDSCONNECTINFO *connect_info);
void tds_set_packet(TDSLOGIN *tds_login, int packet_size);
void tds_set_port(TDSLOGIN *tds_login, int port);
void tds_set_passwd(TDSLOGIN *tds_login, const char *password);
void tds_set_bulk(TDSLOGIN *tds_login, TDS_TINYINT enabled);
void tds_set_user(TDSLOGIN *tds_login, const char *username);
void tds_set_app(TDSLOGIN *tds_login, const char *application);
void tds_set_host(TDSLOGIN *tds_login, const char *hostname);
void tds_set_library(TDSLOGIN *tds_login, const char *library);
void tds_set_server(TDSLOGIN *tds_login, const char *server);
void tds_set_charset(TDSLOGIN *tds_login, const char *charset);
void tds_set_language(TDSLOGIN *tds_login, const char *language);
void tds_set_version(TDSLOGIN *tds_login, short major_ver, short minor_ver);
void tds_set_capabilities(TDSLOGIN *tds_login, unsigned char *capabilities, int size);
int tds_submit_query(TDSSOCKET *tds, const char *query);
int tds_submit_queryf(TDSSOCKET *tds, const char *queryf, ...);
int tds_process_result_tokens(TDSSOCKET *tds, TDS_INT *result_type);
int tds_process_row_tokens(TDSSOCKET *tds, TDS_INT *rowtype, TDS_INT *computeid);
int tds_process_default_tokens(TDSSOCKET *tds, int marker);
TDS_INT tds_process_end(TDSSOCKET *tds, int marker, int *flags);
int tds_client_msg(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, int msgnum, int level, int state, int line, const char *message);
void tds_set_null(unsigned char *current_row, int column);
void tds_clr_null(unsigned char *current_row, int column);
int tds_get_null(unsigned char *current_row, int column);
unsigned char *tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass);
TDSDYNAMIC *tds_lookup_dynamic(TDSSOCKET *tds, char *id);
const char *tds_prtype(int token);

/* iconv.c */
void tds_iconv_open(TDSSOCKET *tds, char *charset);
void tds_iconv_close(TDSSOCKET *tds);
unsigned char *tds7_ascii2unicode(TDSSOCKET *tds, const char *in_string, char *out_string, int maxlen);
char *tds7_unicode2ascii(TDSSOCKET *tds, const char *in_string, char *out_string, int len);
 
/* threadsafe.c */
char *tds_timestamp_str(char *str, int maxlen);
struct hostent *tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct hostent *tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop);
struct servent *tds_getservbyname_r(const char *name, const char *proto, struct servent *result, char *buffer, int buflen);
char *tds_get_homedir(void);

/* mem.c */
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param);
void tds_free_input_params(TDSDYNAMIC *dyn);
void tds_free_dynamic(TDSSOCKET *tds);
TDSSOCKET *tds_realloc_socket(int bufsize);
void tds_free_compute_result(TDSCOMPUTEINFO *comp_info);
void tds_free_compute_results(TDSCOMPUTEINFO **comp_info, TDS_INT num_comp);
unsigned char *tds_alloc_param_row(TDSPARAMINFO *info,TDSCOLINFO *curparam);

/* login.c */
int tds7_send_auth(TDSSOCKET *tds, unsigned char *challenge);

/* query.c */
int tds_submit_prepare(TDSSOCKET *tds, const char *query, const char *id, TDSDYNAMIC **dyn_out);
int tds_submit_execute(TDSSOCKET *tds, TDSDYNAMIC *dyn);
int tds_send_cancel(TDSSOCKET *tds);
const char *tds_next_placeholders(const char *start);
int tds_count_placeholders(const char *query);
int tds_get_dynid(TDSSOCKET *tds, char **id);
int tds_submit_unprepare(TDSSOCKET *tds, TDSDYNAMIC *dyn);
int tds_submit_rpc(TDSSOCKET *tds, const char *rpc_name, TDSPARAMINFO *params);

/* token.c */
int tds_process_cancel(TDSSOCKET *tds);
void tds_swap_datatype(int coltype, unsigned char *buf);
int tds_get_token_size(int marker);
int tds_process_login_tokens(TDSSOCKET *tds);
void tds_set_column_type(TDSCOLINFO *curcol, int type);

/* tds_convert.c */
TDS_INT tds_datecrack(TDS_INT datetype, const void *di, TDSDATEREC *dr);

/* write.c */
int tds_put_bulk_data(TDSSOCKET *tds, const unsigned char *buf, TDS_INT bufsize);
int tds_flush_packet(TDSSOCKET *tds);
int tds_put_buf(TDSSOCKET *tds, const unsigned char *buf, int dsize, int ssize);

/* read.c */
unsigned char tds_get_byte(TDSSOCKET *tds);
void tds_unget_byte(TDSSOCKET *tds);
unsigned char tds_peek(TDSSOCKET *tds);
TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds);
TDS_INT tds_get_int(TDSSOCKET *tds);
char *tds_get_string(TDSSOCKET *tds, void *dest, int n);
char *tds_get_n(TDSSOCKET *tds, void *dest, int n);
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
void tds_answer_challenge(const char *passwd, const char *challenge,TDSANSWER* answer);

#define IS_TDS42(x) (x->major_version==4 && x->minor_version==2)
#define IS_TDS46(x) (x->major_version==4 && x->minor_version==6)
#define IS_TDS50(x) (x->major_version==5 && x->minor_version==0)
#define IS_TDS70(x) (x->major_version==7 && x->minor_version==0)
#define IS_TDS80(x) (x->major_version==8 && x->minor_version==0)

#define IS_TDS7_PLUS(x) ( IS_TDS70(x) || IS_TDS80(x) )

#define IS_TDSDEAD(x) (((x) == NULL) || ((x)->s < 0))

/** Is DB product Sybase ? */
#define TDS_IS_SYBASE(x) (!(x->product_version & 0x80000000u))
/** Is product Microsoft SQL Server ? */
#define TDS_IS_MSSQL(x) ((x->product_version & 0x80000000u)!=0)

/** Get version number for ms sql*/
#define TDS_MS_VER(maj,min,x) (0x80000000u|((maj)<<24)|((min)<<16)|(x))
/* TODO test if not similar to ms one*/
/** Get version number for Sybase*/
#define TDS_SYB_VER(maj,min,x) (((maj)<<24)|((min)<<16)|(x)<<8)

#ifdef __cplusplus
}
#endif 

#endif /* _tds_h_ */
