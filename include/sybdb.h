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
"$Id: sybdb.h,v 1.20 2002-09-16 19:47:58 castellano Exp $";
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
#define INT_CONTINUE 1
#define INT_CANCEL 2
#define INT_TIMEOUT 3

#define DBMAXNUMLEN 33
#define MAXNAME     30

/**
 * DBVERSION_xxx are used with dbsetversion()
 */
#define DBVERSION_UNKNOWN 0
#define DBVERSION_46      1
#define DBVERSION_100     2
#define DBVERSION_42      3
#define DBVERSION_70      4
#define DBVERSION_80      5

/**
 * DBTDS_xxx are returned by DBTDS()
 * The integer values of the constants are poorly chosen.  
 */
#define DBTDS_UNKNOWN           0
#define DBTDS_2_0               1       /* pre 4.0 SQL Server */
#define DBTDS_3_4               2       /* Microsoft SQL Server (3.0) */
#define DBTDS_4_0               3       /* 4.0 SQL Server */
#define DBTDS_4_2               4       /* 4.2 SQL Server */
#define DBTDS_4_6               5       /* 2.0 OpenServer and 4.6 SQL Server. */
#define DBTDS_4_9_5             6       /* 4.9.5 (NCR) SQL Server */
#define DBTDS_5_0               7       /* 5.0 SQL Server */
#define DBTDS_7_0               8       /* Microsoft SQL Server 7.0 */
#define DBTDS_8_0               9       /* Microsoft SQL Server 2000 */

#define SYBAOPCNT  0x4b
#define SYBAOPCNTU 0x4c
#define SYBAOPSUM  0x4d
#define SYBAOPSUMU 0x4e
#define SYBAOPAVG  0x4f
#define SYBAOPAVGU 0x50
#define SYBAOPMIN  0x51
#define SYBAOPMAX  0x52

#define DBTXPLEN 16

#define BCPMAXERRS 1
#define BCPFIRST 2
#define BCPLAST 3
#define BCPBATCH 4

typedef int	 RETCODE;

typedef void	 DBCURSOR;
typedef void	 DBXLATE;
typedef void	 DBSORTORDER;
typedef void	 DBLOGINFO;
typedef void	*DBVOIDPTR;
typedef short	 SHORT;
typedef unsigned short	USHORT;
typedef int	(*INTFUNCPTR)();

#ifndef __INCvxWorksh
/* VxWorks already defines STATUS and BOOL. Compiler gets mad if you 
** redefine them. */
/* __INCvxWorksh will get #defined by std. include files included from tds.h
*/
#ifdef STATUS
/* On DU4.0d we get a conflicting STATUS definition from arpa/nameser.h
   when _REENTRANT is defined.
*/
#undef STATUS
#endif
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
	int	    host_column;
    void    *host_var;
	int	    datatype;
	int	    prefix_len;
	DBINT   column_len;
	BYTE	*terminator;
	int	    term_len;
    int     tab_colnum;
} BCP_HOSTCOLINFO;

typedef struct {
	int	         tab_colnum;
	char	     db_name[256]; /* column name */
	TDS_SMALLINT db_minlen;
	TDS_SMALLINT db_maxlen;
	TDS_SMALLINT db_colcnt; /* I dont know what this does */
	TDS_TINYINT	 db_type;
	TDS_TINYINT	 db_type_save;
	TDS_SMALLINT db_usertype;
	TDS_TINYINT	 db_varint_size;
	TDS_INT		 db_length; /* size of field according to database */
	TDS_TINYINT	 db_nullable;
	TDS_TINYINT	 db_status;
	TDS_SMALLINT db_offset;
	TDS_TINYINT  db_default;
	TDS_TINYINT  db_prec;
	TDS_TINYINT  db_scale;
    TDS_SMALLINT db_flags;
	TDS_INT		 db_size; 
	TDS_TINYINT  db_unicodedata;
    char         db_collate[5];
	long	data_size;
	BYTE	*data;
	int	    txptr_offset;
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
   TDS_CHAR        *bcp_insert_stmt;
   TDS_INT         bcp_direction;
   TDS_INT         bcp_colcount;
   TDS_INT         host_colcount;
   BCP_COLINFO     **bcp_columns;
   BCP_HOSTCOLINFO **host_columns;
   TDS_INT         firstrow;
   TDS_INT         lastrow;
   TDS_INT         maxerrs;
   TDS_INT         bcpbatch;
   TDS_INT         sendrow_init;
   TDS_INT         var_cols;
   DBTYPEINFO      typeinfo;
   unsigned char   avail_flag;
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

typedef int (*MHANDLEFUNC) (DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *proc, int line);

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

#define DBSINGLE 0
#define DBDOUBLE 1
#define DBBOTH   2

DBBOOL db12hour(DBPROCESS *dbprocess, char *language);
BYTE *dbadata(DBPROCESS *dbproc, int computeid, int column);
DBINT dbadlen(DBPROCESS *dbproc, int computeid, int column);
RETCODE dbaltbind(DBPROCESS *dbprocess, int computeid, int column, int vartype, DBINT varlen, BYTE *varaddr);
RETCODE dbaltbind_ps(DBPROCESS *dbprocess, int computeid, int column, int vartype, DBINT varlen, BYTE *varaddr, DBTYPEINFO *typeinfo);
int dbaltcolid(DBPROCESS *dbproc, int computeid, int column);
RETCODE dbaltlen(DBPROCESS *dbproc, int computeid, int column);
int dbaltop(DBPROCESS *dbproc, int computeid, int column);
int dbalttype(DBPROCESS *dbproc, int computeid, int column);
RETCODE dbaltutype(DBPROCESS *dbproc, int computeid, int column);
RETCODE dbanullbind(DBPROCESS *dbprocess, int computeid, int column, DBINT *indicator);
RETCODE dbbind(DBPROCESS *dbproc, int column, int vartype, DBINT varlen, BYTE *varaddr);
RETCODE dbbind_ps(DBPROCESS *dbprocess, int column, int vartype, DBINT varlen, BYTE *varaddr, DBTYPEINFO *typeinfo);
int dbbufsize(DBPROCESS *dbprocess);
BYTE *dbbylist(DBPROCESS *dbproc, int computeid, int *size);
RETCODE dbcancel(DBPROCESS *dbproc);
RETCODE dbcanquery(DBPROCESS *dbproc);
char *dbchange(DBPROCESS *dbprocess);
DBBOOL dbcharsetconv(DBPROCESS *dbprocess);
void dbclose(DBPROCESS *dbproc);
void dbclrbuf(DBPROCESS *dbproc, DBINT n);
RETCODE dbclropt(DBPROCESS *dbproc, int option, char *param);
RETCODE dbcmd(DBPROCESS *dbproc, char *cmdstring);
RETCODE dbcmdrow(DBPROCESS *dbproc);
#define DBCMDROW(x) dbcmdrow((x))
DBBOOL dbcolbrowse(DBPROCESS *dbprocess, int colnum);
DBINT dbcollen(DBPROCESS *dbproc, int column);
char *dbcolname(DBPROCESS *dbproc, int column);
char *dbcolsource(DBPROCESS *dbproc, int colnum);
int dbcoltype(DBPROCESS *dbproc, int column);
int dbcolutype(DBPROCESS *dbproc, int column);
DBTYPEINFO *dbcoltypeinfo(DBPROCESS *dbproc, int column);
DBINT dbcolutype(DBPROCESS *dbprocess, int column);
DBINT dbconvert(DBPROCESS *dbproc, int srctype, BYTE *src, DBINT srclen, int desttype, BYTE *dest, DBINT destlen);
DBINT dbconvert_ps(DBPROCESS *dbprocess, int srctype, BYTE *src, DBINT srclen, int desttype, BYTE *dest, DBINT destlen, DBTYPEINFO *typeinfo);
DBINT dbcount(DBPROCESS *dbproc);
#define DBCOUNT(x) dbcount((x))
int dbcurcmd(DBPROCESS *dbproc);
#define DBCURCMD(x) dbcurcmd((x))
DBINT dbcurrow(DBPROCESS *dbproc);
#define DBCURROW(x) dbcurrow((x))
RETCODE dbcursor(DBCURSOR *hc, DBINT optype, DBINT bufno, BYTE *table, BYTE *values);
RETCODE dbcursorbind(DBCURSOR *hc, int col, int vartype, DBINT varlen, DBINT *poutlen, BYTE *pvaraddr, DBTYPEINFO *typeinfo);
void dbcursorclose(DBCURSOR *hc);
RETCODE dbcursorcolinfo(DBCURSOR *hc, DBINT column, DBCHAR *colname, DBINT *coltype, DBINT *collen, DBINT *usertype);
RETCODE dbcursorfetch(DBCURSOR *hc, DBINT fetchtype, DBINT rownum);
RETCODE dbcursorinfo(DBCURSOR *hc, DBINT *ncols, DBINT *nrows);
DBCURSOR *dbcursoropen(DBPROCESS *dbprocess, BYTE *stmt, SHORT scollopt, SHORT concuropt, USHORT nrows, DBINT *pstatus);
BYTE *dbdata(DBPROCESS *dbproc, int column);
int dbdate4cmp(DBPROCESS *dbprocess, DBDATETIME4 *d1, DBDATETIME4 *d2);
RETCODE dbdate4zero(DBPROCESS *dbprocess, DBDATETIME4 *d1);
RETCODE dbdatechar(DBPROCESS *dbprocess, char *buf, int datepart, int value);
RETCODE dbdatecmp(DBPROCESS *dbproc, DBDATETIME *d1, DBDATETIME *d2);
RETCODE dbdatecrack(DBPROCESS *dbproc, DBDATEREC *di, DBDATETIME *dt);
int dbdatename(DBPROCESS *dbprocess, char *buf, int date, DBDATETIME *datetime);
char *dateorder(DBPROCESS *dbprocess, char *language);
DBINT dbdatepart(DBPROCESS *dbprocess, int datepart, DBDATETIME *datetime);
RETCODE dbdatezero(DBPROCESS *dbprocess, DBDATETIME *d1);
DBINT dbdatlen(DBPROCESS *dbproc, int column);
char *dbdayname(DBPROCESS *dbprocess, char *language, int daynum);
DBBOOL dbdead(DBPROCESS *dbproc);
#define DBDEAD(x) dbdead((x))
EHANDLEFUNC dberrhandle(EHANDLEFUNC handler);
void dbexit(void);
RETCODE dbfcmd(DBPROCESS *dbproc, char *fmt, ...);
DBINT dbfirstrow(DBPROCESS *dbproc);
#define DBFIRSTROW(x) dbfirstrow((x))
RETCODE dbfree_xlate(DBPROCESS *dbprocess, DBXLATE *xlt_tosrv, DBXLATE *clt_todisp);
void dbfreebuf(DBPROCESS *dbproc);
void dbfreequal(char *qualptr);
RETCODE dbfreesort(DBPROCESS *dbprocess, DBSORTORDER *sortorder);
char *dbgetchar(DBPROCESS *dbprocess, int n);
char *dbgetcharset(DBPROCESS *dbprocess);
RETCODE dbgetloginfo(DBPROCESS *dbprocess, DBLOGINFO **loginfo);
int dbgetlusername(LOGINREC *login, BYTE *name_buffer, int buffer_len);
int dbgetmaxprocs(void);
char *dbgetnatlanf(DBPROCESS *dbprocess);
int dbgetoff(DBPROCESS *dbprocess, DBUSMALLINT offtype, int startfrom);
int dbgetpacket(DBPROCESS *dbproc);
RETCODE dbgetrow(DBPROCESS *dbproc, DBINT row);
int DBGETTIME(void);
BYTE *dbgetuserdata(DBPROCESS *dbproc);
DBBOOL dbhasretstat(DBPROCESS *dbproc);
RETCODE dbinit(void);
int dbiordesc(DBPROCESS *dbproc);
#define DBIORDESC(x) dbiordesc((x))
int dbiowdesc(DBPROCESS *dbproc);
#define DBIOWDESC(x) dbiowdesc((x))
DBBOOL dbisavail(DBPROCESS *dbprocess);
#define DBISAVAIL(x) dbisavail((x))
DBBOOL dbisopt(DBPROCESS *dbproc, int option, char *param);
DBINT dblastrow(DBPROCESS *dbproc);
#define DBLASTROW(x) dblastrow((x))
RETCODE dbload_xlate(DBPROCESS *dbprocess, char *srv_charset, char *clt_name, DBXLATE **xlt_tosrv, DBXLATE **xlt_todisp);
DBSORTORDER *dbloadsort(DBPROCESS *dbprocess);
LOGINREC *dblogin(void);
void dbloginfree(LOGINREC *login);
RETCODE dbmny4add(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *sum);
RETCODE dbmny4cmp(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2);
RETCODE dbmny4copy(DBPROCESS *dbprocess, DBMONEY4 *m1, DBMONEY4 *m2);
RETCODE dbmny4divide(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *quotient);
RETCODE dbmny4minus(DBPROCESS *dbproc, DBMONEY4 *src, DBMONEY4 *dest);
RETCODE dbmny4mul(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *prod);
RETCODE dbmny4sub(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *diff);
RETCODE dbmny4zero(DBPROCESS *dbproc, DBMONEY4 *dest);
RETCODE dbmnyadd(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *sum);
RETCODE dbmnycmp(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2);
RETCODE dbmnycopy(DBPROCESS *dbproc, DBMONEY *src, DBMONEY *dest);
RETCODE dbmnydec(DBPROCESS *dbproc, DBMONEY *mnyptr);
RETCODE dbmnydivide(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *quotient);
RETCODE dbmnydown(DBPROCESS *dbproc, DBMONEY *mnyptr, int divisor, int *remainder);
RETCODE dbmnyinc(DBPROCESS *dbproc, DBMONEY *mnyptr);
RETCODE dbmnyinit(DBPROCESS *dbproc, DBMONEY *mnyptr, int trim, DBBOOL *negative);
RETCODE dbmnymaxneg(DBPROCESS *dbproc, DBMONEY *dest);
RETCODE dbmnymaxpos(DBPROCESS *dbproc, DBMONEY *dest);
RETCODE dbmnyminus(DBPROCESS *dbproc, DBMONEY *src, DBMONEY *dest);
RETCODE dbmnymul(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *prod);
RETCODE dbmnydigit(DBPROCESS *dbprocess, DBMONEY *m1, DBCHAR *value, DBBOOL *zero);
RETCODE dbmnyscale(DBPROCESS *dbproc, DBMONEY *dest, int multiplier, int addend);
RETCODE dbmnysub(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *diff);
RETCODE dbmnyzero(DBPROCESS *dbproc, DBMONEY *dest);
char *dbmonthname(DBPROCESS *dbproc, char *language, int monthnum, DBBOOL shortform);
RETCODE dbmorecmds(DBPROCESS *dbproc);
#define DBMORECMDS(x) dbmorecmds((x))
RETCODE dbmoretext(DBPROCESS *dbproc, DBINT size, BYTE *text);
MHANDLEFUNC dbmsghandle(MHANDLEFUNC handler);
char *dbname(DBPROCESS *dbproc);
RETCODE dbnextrow(DBPROCESS *dbproc);
RETCODE dbnpcreate(DBPROCESS *dbprocess);
RETCODE dbnpdefine(DBPROCESS *dbprocess, DBCHAR *procedure_name, DBSMALLINT namelen);
RETCODE dbnullbind(DBPROCESS *dbproc, int column, DBINT *indicator);
int dbnumalts(DBPROCESS *dbproc, int computeid);
int dbnumcols(DBPROCESS *dbproc);
int dbnumcompute(DBPROCESS *dbprocess);
int DBNUMORDERS(DBPROCESS *dbprocess);
int dbnumrets(DBPROCESS *dbproc);
DBPROCESS *tdsdbopen(LOGINREC *login, char *server);
#define   dbopen(x,y) tdsdbopen((x),(y))
int dbordercol(DBPROCESS *dbprocess, int order);
RETCODE dbpoll(DBPROCESS *dbproc, long milliseconds, DBPROCESS **ready_dbproc, int *return_reason);
void dbprhead(DBPROCESS *dbproc);
RETCODE dbprrow(DBPROCESS *dbproc);
char *dbprtype(int token);
char *dbqual(DBPROCESS *dbprocess, int tabnum, char *tabname);
DBBOOL DRBUF(DBPROCESS *dbprocess);
DBINT dbreadpage(DBPROCESS *dbprocess, char *dbname, DBINT pageno, BYTE *buf);
STATUS dbreadtext(DBPROCESS *dbproc, void *buf, DBINT bufsize);
void dbrecftos(char *filename);
RETCODE dbrecvpassthru(DBPROCESS *dbprocess, DBVOIDPTR *bufp);
RETCODE dbregdrop(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen);
RETCODE dbregexec(DBPROCESS *dbproc, DBUSMALLINT options);
RETCODE dbreghandle(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen, INTFUNCPTR handler);
RETCODE dbreginit(DBPROCESS *dbproc, DBCHAR *procedure_name, DBSMALLINT namelen);
RETCODE dbreglist(DBPROCESS *dbproc);
RETCODE dbregnowatch(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen);
RETCODE dbregparam(DBPROCESS *dbproc, char *param_name, int type, DBINT datalen, BYTE *data);
RETCODE dbregwatch(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen, DBUSMALLINT options);
RETCODE dbregwatchlist(DBPROCESS *dbprocess);
RETCODE dbresults(DBPROCESS *dbproc);
BYTE *dbretdata(DBPROCESS *dbproc, int retnum);
int dbretlen(DBPROCESS *dbproc, int retnum);
char *dbretname(DBPROCESS *dbproc, int retnum);
DBINT dbretstatus(DBPROCESS *dbproc);
int dbrettype(DBPROCESS *dbproc, int retnum);
RETCODE dbrows(DBPROCESS *dbproc);
#define DBROWS(x) dbrows((x))
STATUS dbrowtype(DBPROCESS *dbprocess);
#define DBROWTYPE(x) dbrowtype((x))
RETCODE dbrpcinit(DBPROCESS *dbproc, char *rpcname, DBSMALLINT options);
RETCODE dbrpcparam(DBPROCESS *dbproc, char *paramname, BYTE status, int type, DBINT maxlen, DBINT datalen, BYTE *value);
RETCODE dbrpcsend(DBPROCESS *dbproc);
void dbrpwclr(LOGINREC *login);
RETCODE dbrpwset(LOGINREC *login, char *srvname, char *password, int pwlen);
RETCODE dbsafestr(DBPROCESS *dbproc, char *src, DBINT srclen, char *dest, DBINT destlen, int quotetype);
RETCODE *dbsechandle(DBINT type, INTFUNCPTR (*handler)());
RETCODE dbsendpassthru(DBPROCESS *dbprocess, DBVOIDPTR bufp);
char *dbservcharset(DBPROCESS *dbprocess);
void dbsetavail(DBPROCESS *dbprocess);
void dbsetbusy(DBPROCESS *dbprocess, INTFUNCPTR (*handler)());
RETCODE dbsetdefcharset(char *charset);
RETCODE dbsetdeflang(char *language);
void dbsetidle(DBPROCESS *dbprocess, INTFUNCPTR (*handler)());
void dbsetifile(char *filename);
void dbsetinterrupt(DBPROCESS *dbproc, int (*ckintr)(void), int (*hndlintr)(void));
RETCODE dbsetloginfo(LOGINREC *loginrec, DBLOGINFO *loginfo);
RETCODE dbsetlogintime(int seconds);
RETCODE dbsetmaxprocs(int maxprocs);
RETCODE dbsetnull(DBPROCESS *dbprocess, int bindtype, int bindlen, BYTE *bindval);
RETCODE dbsetopt(DBPROCESS *dbproc, int option, char *char_param, int int_param);
STATUS dbsetrow(DBPROCESS *dbprocess, DBINT row);
RETCODE dbsettime(int seconds);
void dbsetuserdata(DBPROCESS *dbproc, BYTE *ptr);
RETCODE dbsetversion(DBINT version);
int dbspid(DBPROCESS *dbproc);
RETCODE dbspr1row(DBPROCESS *dbproc, char *buffer, DBINT buf_len);
DBINT dbspr1rowlen(DBPROCESS *dbproc);
RETCODE dbsprhead(DBPROCESS *dbproc, char *buffer, DBINT buf_len);
RETCODE dbsprline(DBPROCESS *dbproc, char *buffer, DBINT buf_len, DBCHAR line_char);
RETCODE dbsqlexec(DBPROCESS *dbproc);
RETCODE dbsqlok(DBPROCESS *dbproc);
RETCODE dbsqlsend(DBPROCESS *dbproc);
int dbstrbuild(DBPROCESS *dbprocess, char *buf, int size, char *text, char *fmt, ...);
int dbstrcmp(DBPROCESS *dbprocess, char *s1, int l1, char *s2, int l2, DBSORTORDER *sort);
RETCODE dbstrcpy(DBPROCESS *dbproc, int start, int numbytes, char *dest);
int dbstrlen(DBPROCESS *dbproc);
int dbstrsort(DBPROCESS *dbprocess, char *s1, int l1, char *s2, int l2, DBSORTORDER *sort);
DBBOOL dbtabbrowse(DBPROCESS *dbprocess, int tabnum);
int dbtabcount(DBPROCESS *dbprocess);
char *dbtabname(DBPROCESS *dbprocess, int tabnum);
char *dbtabsoruce(DBPROCESS *dbprocess, int colnum, int *tabnum);
#define SYBESMSG        20018   
#define SYBERPND        20019  
#define SYBETIME        20003   /* SQL Server connection timed out. */
#define SYBECLOS        20056   /* Error in closing network connection. */

#define SYBETTS          20066
#define SYBEBDIO         20063
#define SYBEBCVH         20083
#define SYBEBIVI         20080
#define SYBEBCBC         20081
#define SYBEBCFO         20082
#define SYBEBCPB         20078
#define SYBEBCPN         20077
#define SYBEBCPI         20076
#define SYBEBCITBNM      20238
#define SYBEBCITBLEN     20239
#define SYBEBCBNPR       20230
#define SYBEBCBPREF      20236
#define SYBEVDPT         20079
#define SYBEBCPCTYP      20233
#define SYBEBCHLEN       20235
#define SYBEBCPREF       20237
#define SYBEBCVLEN       20234
#define SYBEBCUO         20084
#define SYBEBUOF         20098
#define SYBEBUDF         20102
#define SYBEBIHC         20103
#define SYBEBCUC         20085
#define SYBEBUCF         20099
#define SYBEIFNB         20065
#define SYBEBCRE         20070
#define SYBEBCNN         20073
#define SYBEBBCI         20068

int dbtds(DBPROCESS *dbprocess);
#define DBTDS(a)                dbtds(a)
DBINT dbtextsize(DBPROCESS *dbprocess);
int dbtsnewlen(DBPROCESS *dbprocess);
DBBINARY *dbtsnewval(DBPROCESS *dbprocess);
RETCODE dbtsput(DBPROCESS *dbprocess, DBBINARY *newts, int newtslen, int tabnum, char *tabname);
DBBINARY *dbtxptr(DBPROCESS *dbproc, int column);
DBBINARY *dbtxtimestamp(DBPROCESS *dbproc, int column);
DBBINARY *dbtxtsnewval(DBPROCESS *dbprocess);
RETCODE dbtxtsput(DBPROCESS *dbprocess, DBBINARY newtxts, int colnum);
RETCODE dbuse(DBPROCESS *dbproc, char *dbname);
DBBOOL dbcarylen(DBPROCESS *dbprocess, int column);
const char *dbversion(void);
DBBOOL dbwillconvert(int srctype, int desttype);
RETCODE dbwritepage(DBPROCESS *dbprocess, char *dbname, DBINT pageno, DBINT size, BYTE *buf);
RETCODE dbwritetext(DBPROCESS *dbproc, char *objname, DBBINARY *textptr, DBTINYINT textptrlen, DBBINARY *timestamp, DBBOOL log, DBINT size, BYTE *text);
int dbxlate(DBPROCESS *dbprocess, char *src, int srclen, char *dest, int destlen, DBXLATE *xlt, int *srcbytes_used, DBBOOL srcend, int status);

/* LOGINREC manipulation */
RETCODE dbsetlname(LOGINREC *login, char *value, int which);
RETCODE dbsetlbool(LOGINREC *login, int value, int which);
RETCODE dbsetlshort(LOGINREC *login, int value, int which);
RETCODE dbsetllong(LOGINREC* login, long value, int which);
#define DBSETHOST		1
#define DBSETLHOST(x,y)		dbsetlname((x), (y), DBSETHOST)
#define DBSETUSER		2
#define DBSETLUSER(x,y)		dbsetlname((x), (y), DBSETUSER)
#define DBSETPWD		3
#define DBSETLPWD(x,y)		dbsetlname((x), (y), DBSETPWD)
#define DBSETHID		4 /* not implemented */
#define DBSETLHID(x,y)		dbsetlname((x), (y), DBSETHID)
#define DBSETAPP		5
#define DBSETLAPP(x,y)		dbsetlname((x), (y), DBSETAPP)
#define DBSETBCP		6
#define BCP_SETL(x,y)		dbsetlbool((x), (y), DBSETBCP)
#define DBSETNATLANG		7
#define DBSETLNATLANG(x,y)	dbsetlname((x), (y), DBSETNATLANG)
#define DBSETNOSHORT		8 /* not implemented */
#define DBSETLNOSHORT(x,y)	dbsetlbool((x), (y), DBSETNOSHORT)
#define DBSETHIER		9 /* not implemented */
#define DBSETLHIER(x,y)		dbsetlshort((x), (y), DBSETHIER)
#define DBSETCHARSET		10
#define DBSETLCHARSET(x,y)	dbsetlname((x), (y), DBSETCHARSET)
#define DBSETPACKET		11
#define DBSETLPACKET(x,y)	dbsetllong((x), (y), DBSETPACKET)
#define DBSETENCRYPT		12
#define DBSETLENCRYPT(x,y)	dbsetlbool((x), (y), DBSETENCRYPT)
#define DBSETLABELED		13
#define DBSETLLABELED(x,y)	dbsetlbool((x), (y), DBSETLABELED)
#define BCP_SETLABELED(x,y)	dbsetlbool((x), (y), DBSETLABELED)

RETCODE bcp_init(DBPROCESS *dbproc, char *tblname, char *hfile, char *errfile, int direction);
RETCODE bcp_collen(DBPROCESS *dbproc, DBINT varlen, int table_column);
RETCODE bcp_columns(DBPROCESS *dbproc, int host_colcount);
RETCODE bcp_colfmt(DBPROCESS *dbproc, int host_column, int host_type, int host_prefixlen, DBINT host_collen, BYTE *host_term, int host_termlen, int colnum);
RETCODE bcp_colfmt_ps(DBPROCESS *dbproc, int host_column, int host_type, int host_prefixlen, DBINT host_collen, BYTE *host_term, int host_termlen, int colnum, DBTYPEINFO *typeinfo);
RETCODE bcp_control(DBPROCESS *dbproc, int field, DBINT value);
RETCODE bcp_colptr(DBPROCESS *dbproc, BYTE *colptr, int table_column);
DBBOOL bcp_getl(LOGINREC *login);
RETCODE bcp_sendrow(DBPROCESS *dbproc);
RETCODE bcp_exec(DBPROCESS *dbproc, DBINT *rows_copied);
RETCODE bcp_readfmt(DBPROCESS *dbproc, char *filename);
RETCODE bcp_writefmt(DBPROCESS *dbproc, char *filename);
RETCODE bcp_moretext(DBPROCESS *dbproc, DBINT size, BYTE *text);
RETCODE bcp_batch(DBPROCESS *dbproc);
RETCODE bcp_done(DBPROCESS *dbproc);
RETCODE bcp_bind(DBPROCESS *dbproc, BYTE *varaddr, int prefixlen, DBINT varlen, BYTE *terminator, int termlen, int type, int table_column);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
