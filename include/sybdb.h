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

#ifndef _sybdb_h_
#define _sybdb_h_

#include "tds.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

static char  rcsid_sybdb_h [ ] =
"$Id: sybdb.h,v 1.3 2001-11-08 04:50:36 vorlon Exp $";
static void *no_unused_sybdb_h_warn[]={rcsid_sybdb_h, no_unused_sybdb_h_warn};

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE 0
#define TRUE  1

#define DBSAVE   1
#define DBNOSAVE 0
#define DBNOERR  -1

#define INT_EXIT   0
#define INT_CANCEL 2

#define DBMAXNUMLEN 33
#define MAXNAME     30

#define DBVERSION_UNKNOWN 0
#define DBVERSION_46      1
#define DBVERSION_100     2
#define DBVERSION_42      3
#define DBVERSION_70      4

#define SYBAOPCNT  0x4b
#define SYBAOPCNTU 0x4c
#define SYBAOPSUM  0x4d
#define SYBAOPSUMU 0x4e
#define SYBAOPAVG  0x4f
#define SYBAOPAVGU 0x50
#define SYBAOPMIN  0x51
#define SYBAOPMAX  0x52

#define DBTXPLEN 16

typedef int RETCODE;

#ifndef __INCvxWorksh
/* VxWorks already defines STATUS and BOOL. Compiler gets mad if you 
** redefine them. */
/* __INCvxWorksh will get #defined by std. include files included from tds.h
*/
typedef int STATUS;
typedef unsigned char BOOL ;
#endif

typedef unsigned char DBBOOL ;
typedef TDS_CHAR DBCHAR ;
typedef unsigned char DBTINYINT ;
typedef TDS_SMALLINT DBSMALLINT ;
typedef TDS_INT DBINT	;
typedef unsigned char DBBINARY ;
typedef TDS_REAL DBREAL ;
typedef TDS_FLOAT DBFLT8  ;
typedef unsigned short DBUSMALLINT ;
typedef TDS_NUMERIC DBNUMERIC ;
typedef TDS_MONEY DBMONEY ;
typedef TDS_MONEY4 DBMONEY4 ;
typedef TDS_DATETIME DBDATETIME ;
typedef TDS_DATETIME4 DBDATETIME4 ;

#ifdef MSDBLIB
#define SQLCHAR SYBCHAR
#endif

typedef struct {
void	*tds_login ;
} LOGINREC;

typedef unsigned char BYTE;

typedef struct  dbtypeinfo
{
        DBINT   precision;
        DBINT   scale;
} DBTYPEINFO;

typedef struct tag_DBPROC_ROWBUF
{
   int      buffering_on;    /* (boolean) is row buffering turned on?     */
   int      first_in_buf;    /* result set row number of first row in buf */
   int      next_row;        /* result set row number of next row         */
   int      newest;          /* index of most recent item in queue        */
   int      oldest;          /* index of least recent item in queue       */
   int      elcount;         /* max element count that buffer can hold    */
   int      element_size;    /* size in bytes of each element in queue    */
   int      rows_in_buf;     /* # of rows currently in buffer             */
   void    *rows;            /* pointer to the row storage                */
} DBPROC_ROWBUF;

typedef struct {
	int	column;
	int	datatype;
	int	prefix_len;
	DBINT   column_len;
	BYTE	*terminator;
	int	term_len;
	long	data_size;
	BYTE	*data;
	int	txptr_offset;
	/* fields below here comes from 'insert bulk' result set */
	char	db_name[256]; /* column name */
	TDS_SMALLINT	db_minlen;
	TDS_SMALLINT	db_maxlen;
	TDS_SMALLINT	db_colcnt; /* I dont know what this does */
	TDS_TINYINT	db_type;
	TDS_INT		db_length; /* size of field according to database */
	TDS_TINYINT	db_status;
	TDS_SMALLINT	db_offset;
	TDS_TINYINT	db_default;
	TDS_TINYINT	db_prec;
	TDS_TINYINT	db_scale;
} BCP_COLINFO;

typedef struct {
   TDSSOCKET	  *tds_socket ;
   
   DBPROC_ROWBUF   row_buf;
   
   int             noautofree;
   int             more_results; /* boolean.  Are we expecting results? */
   BYTE           *user_data;   /* see dbsetuserdata() and dbgetuserdata() */
   unsigned char  *dbbuf; /* is dynamic!                   */
   int             dbbufsz;
   int             empty_res_hack;
   TDS_INT         text_size;   
   TDS_INT         text_sent;
   TDS_CHAR        *bcp_hostfile;
   TDS_CHAR        *bcp_errorfile;
   TDS_CHAR        *bcp_tablename;
   TDS_INT         bcp_direction;
   TDS_INT         bcp_colcount;
   BCP_COLINFO     **bcp_columns;
   DBTYPEINFO      typeinfo;
} DBPROCESS;

typedef struct dbdaterec
{
#ifndef MSDBLIB
	DBINT	dateyear;
	DBINT	datemonth;
	DBINT	datedmonth;
	DBINT	datedyear;
	DBINT	datedweek;
	DBINT	datehour;
	DBINT	dateminute;
	DBINT	datesecond;
	DBINT	datemsecond;
	DBINT	datetzone;
#else
	DBINT	year;
	DBINT	month;
	DBINT	day;
	DBINT	dayofyear;
	DBINT	weekday;
	DBINT	hour;
	DBINT	minute;
	DBINT	second;
	DBINT	millisecond;
	DBINT	tzone;
#endif
} DBDATEREC;

typedef int (*EHANDLEFUNC) (DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);

typedef int (*MHANDLEFUNC) (DBPROCESS *dbproc, int msgno, int msgstate, int severity, char *msgtext, char *srvname, char *proc, int line);

enum {
	DBPADOFF,
	DBPADON
};
/* a large list of options, DBTEXTSIZE is needed by sybtcl */
#define DBPARSEONLY    0
#define DBESTIMATE     1
#define DBSHOWPLAN     2
#define DBNOEXEC       3
#define DBARITHIGNORE  4
#define DBNOCOUNT      5
#define DBARITHABORT   6
#define DBTEXTLIMIT    7
#define DBBROWSE       8
#define DBOFFSET       9
#define DBSTAT        10
#define DBERRLVL      11
#define DBCONFIRM     12
#define DBSTORPROCID  13
#define DBBUFFER      14
#define DBNOAUTOFREE  15
#define DBROWCOUNT    16
#define DBTEXTSIZE    17
#define DBNATLANG     18
#define DBDATEFORMAT  19
#define DBPRPAD       20
#define DBPRCOLSEP    21
#define DBPRLINELEN   22
#define DBPRLINESEP   23
#define DBLFCONVERT   24
#define DBDATEFIRST   25
#define DBCHAINXACTS  26
#define DBFIPSFLAG    27
#define DBISOLATION   28
#define DBAUTH        29
#define DBIDENTITY    30

#define DBNUMOPTIONS  31

#define DBPRPADON      1
#define DBPRPADOFF     0

/* dbpoll() result codes, sybtcl needs DBRESULT */
#define DBRESULT       1
#define DBNOTIFICATION 2
#define DBTIMEOUT      3
#define DBINTERRUPT    4

/* more sybtcl needs: */
#define DBTXTSLEN    8

/* bind types */
#define CHARBIND          0
#define STRINGBIND        1
#define NTBSTRINGBIND     2
#define VARYCHARBIND      3
#define TINYBIND          6
#define SMALLBIND         7
#define INTBIND           8
#define FLT8BIND          9
#define REALBIND          10
#define DATETIMEBIND      11
#define SMALLDATETIMEBIND 12
#define MONEYBIND         13
#define SMALLMONEYBIND    14
#define BINARYBIND        15
#define BITBIND           16
#define NUMERICBIND       17
#define DECIMALBIND       18

#define DBPRCOLSEP  21
#define DBPRLINELEN 22
#define DBRPCRETURN 1

#define REG_ROW         -1
#define MORE_ROWS       -1
#define NO_MORE_ROWS    -2
#define BUF_FULL        -3
#define NO_MORE_RESULTS 2
#define SUCCEED         1
#define FAIL            0

#define DB_IN  1
#define DB_OUT 2
#define IN     1
#define OUT    2

#define DBSINGLE 0
#define DBDOUBLE 1
#define DBBOTH   2

extern	RETCODE    dbinit();
extern	LOGINREC  *dblogin();
extern	RETCODE    DBSETLPACKET(LOGINREC *login, short packet_size);
extern	RETCODE    DBSETLPWD(LOGINREC *login, char *password);
extern	RETCODE    DBSETLUSER(LOGINREC *login, char *username);
extern	RETCODE    DBSETLHOST(LOGINREC *login, char *hostname);
extern	RETCODE    DBSETLAPP(LOGINREC *login, char *application);
extern	DBPROCESS *tdsdbopen(LOGINREC *login,char *server);
#define   dbopen(x,y) tdsdbopen(x,y)
extern  RETCODE    dbclose(DBPROCESS *dbprocess);
extern	DBINT      dbconvert(DBPROCESS *dbproc, int srctype, 
                             BYTE *src, DBINT srclen, int desttype, BYTE *dest,
                             DBINT destlen);
extern	void       dbexit();
extern	RETCODE    DBCMDROW(DBPROCESS *dbproc);
extern	RETCODE    dbsetdeflang(char *language);
extern	int        dbgetpacket(DBPROCESS *dbproc);
extern	RETCODE    dbsetmaxprocs(int maxprocs);
extern	RETCODE    dbsettime(int seconds);
extern	RETCODE    dbsetlogintime(int seconds);
extern  RETCODE    dbresults(DBPROCESS *dbproc);
extern  RETCODE    dbnextrow(DBPROCESS *dbproc);
extern  RETCODE    dbgetrow(DBPROCESS *dbproc, DBINT row);
extern	DBINT      dbretstatus(DBPROCESS *dbproc);
extern	DBBOOL     dbhasretstat(DBPROCESS *dbproc);
extern	DBINT      dbdatlen(DBPROCESS *dbproc, int column);
extern	char      *dbcolsource(DBPROCESS *dbproc,int colnum);
extern	int        dbcoltype(DBPROCESS *dbproc,int column);
extern	DBINT      DBCOUNT(DBPROCESS *dbproc);
extern  DBINT      DBLASTROW(DBPROCESS *dbproc);
extern  void       dbclrbuf(DBPROCESS *dbproc, DBINT n);
extern  DBBOOL     dbwillconvert(int srctype, int desttype);
extern	int        dbaltcolid(DBPROCESS *dbproc, int computeid, int column);
extern	DBINT      dbadlen(DBPROCESS *dbproc,int computeid, int column);
extern	int        dbalttype(DBPROCESS *dbproc, int computeid, int column);
extern	BYTE      *dbadata(DBPROCESS *dbproc, int computeid, int column);
extern	int        dbaltop(DBPROCESS *dbproc, int computeid, int column);
extern	RETCODE    dbsetopt(DBPROCESS *dbproc, int option, char *char_param, 
                            int int_param);
extern	void       dbsetinterrupt(DBPROCESS *dbproc, 
                                  int (*ckintr)(),int (*hndlintr)());
extern	RETCODE    dbcancel(DBPROCESS *dbproc);
extern	int        dbnumrets(DBPROCESS *dbproc);
extern	char      *dbretname(DBPROCESS *dbproc, int retnum);
extern	BYTE      *dbretdata(DBPROCESS *dbproc, int retnum);
extern	int        dbretlen(DBPROCESS *dbproc, int retnum);
extern	RETCODE    dbsqlok(DBPROCESS *dbproc);
extern	void       dbprrow(DBPROCESS *dbproc);
extern	void       dbprhead(DBPROCESS *dbproc);
extern	void       dbloginfree(LOGINREC *login);
extern	int        dbnumalts(DBPROCESS *dbproc,int computeid);
extern	BYTE      *dbbylist(DBPROCESS *dbproc, int computeid, int size);
extern	RETCODE    dbrpcinit(DBPROCESS *dbproc,char *rpcname,
                             DBSMALLINT options);
extern	RETCODE    dbrpcparam(DBPROCESS *dbproc, char *paramname, BYTE status,
                              int type, DBINT maxlen, DBINT datalen, 
                              BYTE *value);
extern	RETCODE    dbrpcsend(DBPROCESS *dbproc);
extern	RETCODE    dbuse(DBPROCESS *dbproc,char *dbname);
extern	DBBOOL     DBDEAD(DBPROCESS *dbproc);

extern	int (*dbmsghandle( int (*handler)() )) ();
extern	int (*dberrhandle( int (*handler)() )) ();

extern	RETCODE    BCP_SETL(LOGINREC *login, DBBOOL enable);
extern	RETCODE    bcp_init(DBPROCESS *dbproc, char *tblname, char *hfile, 
                            char *errfile, int direction);
extern	RETCODE    bcp_collen(DBPROCESS *dbproc, DBINT varlen, 
                              int table_column);
extern	RETCODE    bcp_columns(DBPROCESS *dbproc, int host_colcount);
extern	RETCODE    bcp_colfmt(DBPROCESS *dbproc, int host_colnum, 
                              int host_type, int host_prefixlen, 
                              DBINT host_collen, BYTE *host_term,
                              int host_termlen, int table_colnum);
extern	RETCODE    bcp_colfmt_ps(DBPROCESS *dbproc, int host_colnum, 
                                 int host_type, int host_prefixlen, 
                                 DBINT host_collen, BYTE *host_term,
                                 int host_termlen, int table_colnum, 
                                 DBTYPEINFO *typeinfo);
extern	RETCODE    bcp_control(DBPROCESS *dbproc, int field, DBINT value);
extern	RETCODE    bcp_colptr(DBPROCESS *dbproc, BYTE *colptr,
                              int table_column);
extern	DBBOOL     bcp_getl(LOGINREC *login);
extern	RETCODE    bcp_exec(DBPROCESS *dbproc, DBINT *rows_copied);
extern	RETCODE    bcp_readfmt(DBPROCESS *dbproc, char *filename);
extern	RETCODE    bcp_writefmt(DBPROCESS *dbproc, char *filename);
extern	RETCODE    bcp_sendrow(DBPROCESS *dbproc);
extern	RETCODE    bcp_moretext(DBPROCESS *dbproc, DBINT size, BYTE *text);
extern	RETCODE    bcp_batch(DBPROCESS *dbproc);
extern	RETCODE    bcp_done(DBPROCESS *dbproc);
extern	RETCODE    bcp_bind(DBPROCESS *dbproc, BYTE *varaddr, int prefixlen, 
                            DBINT varlen, BYTE *terminator, int termlen, 
                            int type, int table_column);
extern	RETCODE    dbmnyadd(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, 
                            DBMONEY *sum);
extern	RETCODE    dbmnysub(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, 
                            DBMONEY *diff);
extern	RETCODE    dbmnymul(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2,
                            DBMONEY *prod);
extern	RETCODE    dbmnydivide(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2,
                               DBMONEY *quotient);
extern	RETCODE    dbmnycmp(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2);
extern	RETCODE    dbmnyscale(DBPROCESS *dbproc, DBMONEY *dest,int multiplier,
                              int addend);
extern	RETCODE    dbmnyzero(DBPROCESS *dbproc, DBMONEY *dest);
extern	RETCODE    dbmnymaxpos(DBPROCESS *dbproc, DBMONEY *dest);
extern	RETCODE    dbmnymaxneg(DBPROCESS *dbproc, DBMONEY *dest);
extern	RETCODE    dbmnyndigit(DBPROCESS *dbproc, DBMONEY *mnyptr,
                               DBCHAR *value, DBBOOL *zero);
extern	RETCODE    dbmnyinit(DBPROCESS *dbproc,DBMONEY *mnyptr, int trim, 
                             DBBOOL *negative);
extern	RETCODE    dbmnydown(DBPROCESS *dbproc,DBMONEY *mnyptr, int divisor, 
                             int *remainder);
extern	RETCODE    dbmnyinc(DBPROCESS *dbproc,DBMONEY *mnyptr);
extern	RETCODE    dbmnydec(DBPROCESS *dbproc,DBMONEY *mnyptr);
extern	RETCODE    dbmnyminus(DBPROCESS *dbproc,DBMONEY *src, DBMONEY *dest);
extern	RETCODE    dbmny4cmp(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2);
extern	RETCODE    dbmny4minus(DBPROCESS *dbproc, DBMONEY4 *src, 
                               DBMONEY4 *dest);
extern	RETCODE    dbmny4zero(DBPROCESS *dbproc, DBMONEY4 *dest);
extern	RETCODE    dbmny4add(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, 
                             DBMONEY4 *sum);
extern	RETCODE    dbmny4sub(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2,
                             DBMONEY4 *diff);
extern	RETCODE    dbmny4mul(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, 
                             DBMONEY4 *prod);
extern	RETCODE    dbmny4divide(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2,
                                DBMONEY4 *quotient);
extern	RETCODE    dbmny4cmp(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2);
extern	RETCODE    dbdatecmp(DBPROCESS *dbproc, DBDATETIME *d1,
                             DBDATETIME *d2);
extern	RETCODE    dbdatecrack(DBPROCESS *dbproc, DBDATEREC *dateinfo,
                               DBDATETIME *datetime);
extern	void       dbrpwclr(LOGINREC *login);
extern	RETCODE    dbrpwset(LOGINREC *login, char *srvname, char *password, 
                            int pwlen);
extern	void       build_xact_string(char *xact_name, char *service_name,
                                     DBINT commid, char *result);
extern	RETCODE    remove_xact(DBPROCESS *connect, DBINT commid, int n);
extern	RETCODE    abort_xact(DBPROCESS *connect, DBINT commid);
extern	void       close_commit(DBPROCESS *connect);
extern	RETCODE    commit_xact(DBPROCESS *connect, DBINT commid);
extern	DBPROCESS *open_commit(LOGINREC *login, char *servername);
extern	RETCODE    scan_xact(DBPROCESS *connect, DBINT commid);
extern	DBINT      start_xact(DBPROCESS *connect, char *application_name,
                              char *xact_name, int site_count);
extern	DBINT      stat_xact(DBPROCESS *connect, DBINT commid);
extern	int        dbspid(DBPROCESS *dbproc);
extern  char      *dbmonthname(DBPROCESS *dbproc,char *language,int monthnum,
                               DBBOOL shortform);
extern  char      *dbname(DBPROCESS *dbproc);
extern  BYTE      *dbdata(DBPROCESS *dbproc,int column);
extern  char      *dbcolname(DBPROCESS *dbproc,int column);
extern  DBBINARY  *dbtxptr(DBPROCESS *dbproc,int column);
extern  DBBINARY  *dbtxtimestamp(DBPROCESS *dbproc, int column);
extern  RETCODE    dbwritetext(DBPROCESS *dbproc,char *objname, 
				DBBINARY *textptr, DBTINYINT textptrlen, 
				DBBINARY *timestamp, DBBOOL log, 
                                        DBINT size, BYTE *text);
extern  void       dbfreebuf(DBPROCESS *dbproc);
extern  RETCODE	   dbcmd(DBPROCESS *dbproc, char *cmdstring);
extern  RETCODE    dbsqlexec(DBPROCESS *dbproc);
extern  int        dbnumcols(DBPROCESS *dbproc);
extern  DBINT      dbcollen(DBPROCESS *dbproc, int column);
extern  DBTYPEINFO *dbcoltypeinfo(DBPROCESS *dbproc, int column);
extern  char      *dbprtype(int token);
extern  RETCODE    dbbind(DBPROCESS *dbproc, int column, int vartype, 
                          DBINT varlen, BYTE *varaddr);
extern  RETCODE	   dbnullbind(DBPROCESS *dbproc, int column, DBINT *indicator);
extern  RETCODE    dbsqlsend(DBPROCESS *dbproc);
extern  RETCODE    dbaltutype(DBPROCESS *dbproc, int computeid, int column);
extern  RETCODE    dbaltlen(DBPROCESS *dbproc, int computeid, int column);
extern  RETCODE    dbpoll(DBPROCESS *dbproc, long milliseconds,
                          DBPROCESS **ready_dbproc, int *return_reason);

extern	int        DBIORDESC(DBPROCESS *dbproc);
extern	int        DBIOWDESC(DBPROCESS *dbproc);

extern  DBINT dbspr1rowlen(DBPROCESS *dbproc);
extern  RETCODE dbspr1row(DBPROCESS *dbproc, char *buffer, DBINT buf_len);
extern  RETCODE dbsprline(DBPROCESS *dbproc,char *buffer, DBINT buf_len, 
			DBCHAR line_char);
extern  RETCODE dbsprhead(DBPROCESS *dbproc,char *buffer, DBINT buf_len);
extern  char *dbversion();
extern  RETCODE dbcanquery(DBPROCESS *dbproc);

/* added to allow propery error handling (mlilback, 11/17/01)*/
typedef int (*dberrhandle_func)(DBPROCESS *dbproc, int severity, int dberr, 
      int oserr, char *dberrstr, char *oserrstr);
typedef int (*dbmsghandle_func)(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, 
      char *msgtext, char *srvname, char *procname, int line);

extern        dberrhandle_func        dberrhandler(dberrhandle_func handler);
extern        dbmsghandle_func        dbmsghandler(dbmsghandle_func handler);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
