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
	 "$Id: tds.h,v 1.4 2001-10-24 03:42:04 brianb Exp $";
static void *no_unused_tds_h_warn[]={rcsid_tds_h, no_unused_tds_h_warn};

#include "tds_configs.h"

#if HAVE_ICONV
#include <iconv.h>
#endif
#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/signal.h>
#include <sys/wait.h>
#endif 

#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#endif

#ifdef __INCvxWorksh
#include <signal.h>
#include <ioLib.h> /* for FIONBIO */
#else
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
/* FIX ME -- use autoconf for the existance of malloc.h */
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <sys/time.h>
#endif

#ifndef WIN32
#include <unistd.h>
#endif

#include "tdsver.h"


#ifdef __cplusplus
extern "C" {
#endif 

/* 
** this is where platform specific changes need to be made.
** I've tried to change all references to data that goes to 
** or comes off the wire to use these typedefs.  I've probably 
** missed a bunch, but the idea is we can do an ifdef here 
** to set the appropriately sized native type.
**
** If you have problems on 64bit machines and the code is 
** using a native datatype, please change the code to use
** these. (In the TDS layer only, the API layers have their
** own typedefs which equate to these).
*/
typedef char		TDS_CHAR;      /*  8 bit char     */
typedef unsigned char	TDS_UCHAR;     /*  8 bit uchar    */
typedef unsigned char	TDS_TINYINT;   /*  8 bit int      */
typedef short             TDS_SMALLINT;  /* 16 bit int      */
typedef unsigned short  TDS_USMALLINT; /* 16 bit unsigned */
typedef int                  TDS_INT;       /* 32 bit int      */
typedef unsigned int       TDS_UINT;      /* 32 bit unsigned */
typedef float                 TDS_REAL;      /* 32 bit float    */
typedef double                TDS_FLOAT;     /* 64 bit float    */
typedef struct tdsnumeric
{
        unsigned char         precision;
        unsigned char         scale;
        unsigned char         array[33];
} TDS_NUMERIC;
typedef struct tdsmoney
{
        TDS_INT         mnyhigh;
        TDS_INT         mnylow;
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

#define TDS_NO_MORE_ROWS     -2
#define TDS_SUCCEED          1
#define TDS_FAIL             0
#define TDS_NO_MORE_RESULTS  2
/*
** TDS_ERROR indicates a successful processing, but an TDS_ERR_TOKEN or 
** TDS_EED_TOKEN error was encountered, whereas TDS_FAIL indicates an
** unrecoverable failure.
*/
#define TDS_ERROR            3  
#define TDS_DONT_RETURN      42

#define TDS5_DYN_TOKEN      231  /* 0xE7    TDS 5.0 only              */
#define TDS5_DYNRES_TOKEN   236  /* 0xEC    TDS 5.0 only              */
#define TDS5_DYN3_TOKEN     215  /* 0xD7    TDS 5.0 only              */
#define TDS_LANG_TOKEN       33  /* 0x21    TDS 5.0 only              */
#define TDS_CLOSE_TOKEN     113  /* 0x71    TDS 5.0 only? ct_close()  */
#define TDS_RET_STAT_TOKEN  121  /* 0x79                              */
#define TDS_124_TOKEN       124  /* 0x7C    TDS 4.2 only - TDS_PROCID */
#define TDS7_RESULT_TOKEN   129  /* 0x81    TDS 7.0 only              */
#define TDS_COL_NAME_TOKEN  160  /* 0xA0    TDS 4.2 only              */
#define TDS_COL_INFO_TOKEN  161  /* 0xA1    TDS 4.2 only - TDS_COLFMT */
/*#define  TDS_TABNAME   164 */
/*#define  TDS_COL_INFO   165 */
#define TDS_167_TOKEN       167  /* 0xA7                              */
#define TDS_168_TOKEN       168  /* 0xA8                              */
#define TDS_ORDER_BY_TOKEN  169  /* 0xA9    TDS_ORDER                 */
#define TDS_ERR_TOKEN       170  /* 0xAA                              */
#define TDS_MSG_TOKEN       171  /* 0xAB                              */
#define TDS_PARAM_TOKEN     172  /* 0xAC    RETURNVALUE?              */
#define TDS_LOGIN_ACK_TOKEN 173  /* 0xAD                              */
#define TDS_174_TOKEN       174  /* 0xAE    TDS_CONTROL               */
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
#define CANCEL          2
#define GOTO_1ST_ROW    3
#define LOGIN           4

/* environment type field */
#define TDS_ENV_DATABASE  1
#define TDS_ENV_CHARSET   3
#define TDS_ENV_BLOCKSIZE 4

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
#define XSYBCHAR     167  /* 0xA7 */
#define XSYBVARCHAR  175  /* 0xAF */
#define XSYBNVARCHAR 231  /* 0xE7 */
#define XSYBNCHAR    239  /* 0xEF */
#define SYBUNIQUE    0x24
#define SYBVARIANT   0x62

#define TDS_ZERO_FREE(x) {free((x)); (x) = NULL;}

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
#define is_unicode(x) (x==XSYBNVARCHAR || x==XSYBNCHAR)

#define TDS_MAX_CAPABILITY	18
#define MAXPRECISION 		50
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
#define TDS_STR_SWAPMNY  "tds version"
#define TDS_STR_TRYSVR   "try server login"
#define TDS_STR_TRYDOM   "try domain login"
#define TDS_STR_DOMAIN   "nt domain"
#define TDS_STR_XDOMAUTH "cross domain auth"
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
#define TDS_STR_LANGUAGE	"language"

#define TDS_MAX_LOGIN_STR_SZ 30
typedef struct tds_login {
	TDS_CHAR  host_name[TDS_MAX_LOGIN_STR_SZ+1];
	TDS_CHAR  user_name[TDS_MAX_LOGIN_STR_SZ+1];
	TDS_CHAR  password[TDS_MAX_LOGIN_STR_SZ+1];
	TDS_TINYINT bulk_copy; 
	TDS_CHAR  app_name[TDS_MAX_LOGIN_STR_SZ+1];
	TDS_CHAR  server_name[TDS_MAX_LOGIN_STR_SZ+1];
	TDS_TINYINT  major_version; /* TDS version */
	TDS_TINYINT  minor_version; /* TDS version */
	TDS_CHAR  library[11]; /* Ct-Library or DB-Library */
	TDS_CHAR language[TDS_MAX_LOGIN_STR_SZ+1]; /* ie us-english */
	TDS_TINYINT encrypted; 
	TDS_CHAR char_set[TDS_MAX_LOGIN_STR_SZ+1]; /*  ie iso_1 */
	TDS_SMALLINT block_size; 
	TDS_TINYINT suppress_language;
     TDS_INT connect_timeout;
     TDS_INT query_timeout;
     TDS_INT longquery_timeout;
     void (*longquery_func)(long lHint);
     long longquery_param;
	unsigned char capabilities[TDS_MAX_CAPABILITY];
	int port;
} TDSLOGIN;

typedef struct tds_config_info {
        char *server_name;
        char *host;
        char *ip_addr;
        int port;
        TDS_SMALLINT minor_version;
        TDS_SMALLINT major_version;
        int block_size;
        char *language;
        char *char_set;
        char *database;
        char *dump_file;
        int broken_dates;
        int broken_money;
        int timeout;
        int connect_timeout;
        char *host_name;
        char *default_domain;
        int try_server_login;
        int try_domain_login;
        int xdomain_auth;
        int debug_level;
        int emul_little_endian;
        int text_size;
        char *app_name;
        char *user_name;
        char *password;
        char *library;
        int bulk_copy;
	   int suppress_language;
	   int encrypted;
} TDSCONFIGINFO;

/* structure for storing data about regular and compute rows */ 
typedef struct tds_column_info {
	TDS_SMALLINT column_type;
	TDS_SMALLINT column_xtype;
	TDS_SMALLINT column_usertype;
	TDS_INT column_size;
	TDS_INT column_offset;
	TDS_CHAR column_name[256];
	TDS_SMALLINT column_bindtype;
	TDS_SMALLINT column_bindfmt;
	TDS_UINT column_bindlen;
	TDS_CHAR *column_nullbind;
	TDS_CHAR *varaddr;
	TDS_CHAR *column_lenbind;
	TDS_SMALLINT column_prec;
	TDS_SMALLINT column_scale;
	TDS_INT column_textsize;
	TDS_INT column_textpos;
	TDS_CHAR column_textptr[16];
	TDS_CHAR column_timestamp[8];
	TDS_CHAR *column_textvalue;
} TDSCOLINFO;

typedef struct tds_result_info {
	TDS_SMALLINT  rows_exist;
	TDS_INT       row_count;
	TDS_INT       row_size;
	TDS_SMALLINT  num_cols;
	TDS_TINYINT   more_results;
	TDSCOLINFO    **columns;
	int           null_info_size;
	unsigned char *current_row;
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

typedef struct tds_compute_info {
        TDS_SMALLINT num_cols;
	TDS_INT row_size;
        TDSCOLINFO **columns;
	int           null_info_size;
	unsigned char *current_row;
} TDSCOMPUTEINFO;

typedef struct tds_param_info {
        TDS_SMALLINT num_cols;
	TDS_INT row_size;
        TDSCOLINFO **columns;
	int           null_info_size;
	unsigned char *current_row;
} TDSPARAMINFO;

typedef struct tds_msg_info {
      TDS_SMALLINT priv_msg_type;
      TDS_SMALLINT line_number;
      TDS_SMALLINT msg_number;
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
	TDSRESULTINFO *res_info;
} TDSDYNAMIC;

typedef struct tds_socket {
	/* fixed and connect time */
        int s;
	TDS_SMALLINT major_version;
	TDS_SMALLINT minor_version;
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
	TDSRESULTINFO *res_info;
        TDSCOMPUTEINFO *comp_info;
        TDSPARAMINFO *param_info;
	TDS_TINYINT   has_status;
	TDS_INT       ret_status;
	TDSMSGINFO *msg_info;
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
	int cur_dyn_elem;
	TDSDYNAMIC **dyns;
	int emul_little_endian;
	int use_iconv;
#if HAVE_ICONV
	iconv_t cdto;
	iconv_t cdfrom;
#endif
} TDSSOCKET;

typedef struct tds_context {
	TDSSOCKET *connection_list[TDS_MAX_CONN];
} TDSCONTEXT;

void tds_set_longquery_handler(TDSLOGIN * tds_login, void * longquery_func, long longquery_param);
void tds_set_timeouts(TDSLOGIN *tds_login, int connect, int query, int longquery);
extern int tds_write_packet(TDSSOCKET *tds,unsigned char final);
extern int tds_init_write_buf(TDSSOCKET *tds);
extern void tds_free_result_info(TDSRESULTINFO *info);
extern void tds_free_socket(TDSSOCKET *tds);
extern void tds_free_config(TDSCONFIGINFO *config);
extern void tds_free_all_results(TDSSOCKET *tds);
extern void tds_free_results(TDSRESULTINFO *res_info);
extern void tds_free_param_results(TDSPARAMINFO *param_info);
extern void tds_free_column(TDSCOLINFO *column);
extern void tds_free_msg(TDSMSGINFO *msg_info);
extern int tds_put_n(TDSSOCKET *tds, unsigned char *buf, int n);
extern int tds_put_string(TDSSOCKET *tds, unsigned char *buf, int n);
extern int tds_put_int(TDSSOCKET *tds, TDS_INT i);
extern int tds_put_smallint(TDSSOCKET *tds, TDS_SMALLINT si);
extern int tds_put_tinyint(TDSSOCKET *tds, TDS_TINYINT ti);
extern int tds_put_byte(TDSSOCKET *tds, unsigned char c);
extern unsigned char tds_get_byte(TDSSOCKET *tds);
extern void tds_unget_byte(TDSSOCKET *tds);
extern TDS_INT tds_get_int(TDSSOCKET *tds);
extern TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds);
extern char *tds_get_n(TDSSOCKET *tds, void *dest, int n);
extern char *tds_get_string(TDSSOCKET *tds, void *dest, int n);
extern TDSRESULTINFO *tds_alloc_results(int num_cols);
extern TDSCOMPUTEINFO *tds_alloc_compute_results(int num_cols);
extern TDSSOCKET *tds_alloc_socket(int bufsize);
extern TDSCONFIGINFO *tds_get_config(TDSSOCKET *tds, TDSLOGIN *login);
extern void *tds_alloc_row(TDSRESULTINFO *res_info);
extern char *tds_msg_get_proc_name(TDSSOCKET *tds);
extern TDSLOGIN *tds_alloc_login();
extern TDSCONFIGINFO *tds_alloc_config();
extern TDSSOCKET *tds_connect(TDSLOGIN *login);
extern void tds_set_packet(TDSLOGIN *tds_login, short packet_size);
extern void tds_set_port(TDSLOGIN *tds_login, int port);
extern void tds_set_passwd(TDSLOGIN *tds_login, char *password);
extern void tds_set_bulk(TDSLOGIN *tds_login, TDS_TINYINT enabled);
extern void tds_set_user(TDSLOGIN *tds_login, char *username);
extern void tds_set_app(TDSLOGIN *tds_login, char *application);
extern void tds_set_host(TDSLOGIN *tds_login, char *hostname);
extern void tds_set_library(TDSLOGIN *tds_login, char *library);
extern void tds_set_server(TDSLOGIN *tds_login, char *server);
extern void tds_set_charset(TDSLOGIN *tds_login, char *charset);
extern void tds_set_language(TDSLOGIN *tds_login, char *language);
extern void tds_set_version(TDSLOGIN *tds_login, short major_ver, short minor_ver);
extern void tds_set_capabilities(TDSLOGIN *tds_login, unsigned char *capabilities, int size);
extern int tds_submit_query(TDSSOCKET *tds, char *query);
extern int tds_process_result_tokens(TDSSOCKET *tds);
extern int tds_process_row_tokens(TDSSOCKET *tds);
extern int tds_process_env_chg(TDSSOCKET *tds);
extern int tds_process_default_tokens(TDSSOCKET *tds, int marker);
extern TDS_INT tds_process_end(TDSSOCKET *tds, int marker, int *more, int *canceled);
extern int tds_client_msg(TDSSOCKET *tds, int msgnum, int level, int state, int line, char *message);
extern int tds_reset_msg_info(TDSSOCKET *tds);
extern void  tds_set_parent(TDSSOCKET* tds, void* the_parent);
extern void* tds_get_parent(TDSSOCKET* tds);
extern void tds_set_null(unsigned char *current_row, int column);
extern void tds_clr_null(unsigned char *current_row, int column);
extern int tds_get_null(unsigned char *current_row, int column);
extern int tds7_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config);
extern char *tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass);

/* iconv.c */
void tds_iconv_open(TDSSOCKET *tds, char *charset);
void tds_iconv_close(TDSSOCKET *tds);
extern char *tds7_ascii2unicode(TDSSOCKET *tds, const char *in_string, char *out_string, int maxlen);
extern char *tds7_unicode2ascii(TDSSOCKET *tds, const char *in_string, char *out_string, int len);
 

#define IS_TDS42(x) (x->major_version==4 && x->minor_version==2)
#define IS_TDS46(x) (x->major_version==4 && x->minor_version==6)
#define IS_TDS50(x) (x->major_version==5 && x->minor_version==0)
#define IS_TDS70(x) (x->major_version==7 && x->minor_version==0)
#define IS_TDS80(x) (x->major_version==8 && x->minor_version==0)


#ifdef __cplusplus
}
#endif 

#endif /* _tds_h_ */
