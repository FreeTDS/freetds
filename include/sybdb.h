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
"$Id: sybdb.h,v 1.4 2002-01-31 02:21:44 brianb Exp $";
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

#define DBSINGLE 0
#define DBDOUBLE 1
#define DBBOTH   2

extern DBBOOL db12hour(DBPROCESS *dbprocess, char *language);
extern BYTE *dbadata(DBPROCESS *dbproc, int computeid, int column);
extern DBINT dbadlen(DBPROCESS *dbproc, int computeid, int column);
extern RETCODE dbaltbind(DBPROCESS *dbprocess, int computeid, int column, int vartype, DBINT varlen, BYTE *varaddr);
extern RETCODE dbaltbind_ps(DBPROCESS *dbprocess, int computeid, int column, int vartype, DBINT varlen, BYTE *varaddr, DBTYPEINFO *typeinfo);
extern int dbaltcolid(DBPROCESS *dbproc, int computeid, int column);
extern RETCODE dbaltlen(DBPROCESS *dbproc, int computeid, int column);
extern int dbaltop(DBPROCESS *dbproc, int computeid, int column);
extern int dbalttype(DBPROCESS *dbproc, int computeid, int column);
extern RETCODE dbaltutype(DBPROCESS *dbproc, int computeid, int column);
extern RETCODE dbanullbind(DBPROCESS *dbprocess, int computeid, int column, DBINT *indicator);
extern RETCODE dbbind(DBPROCESS *dbproc, int column, int vartype, DBINT varlen, BYTE *varaddr);
extern RETCODE dbbind_ps(DBPROCESS *dbprocess, int column, int vartype, DBINT varlen, BYTE *varaddr, DBTYPEINFO *typeinfo);
extern int dbbufsize(DBPROCESS *dbprocess);
extern BYTE *dbbylist(DBPROCESS *dbproc, int computeid, int *size);
extern RETCODE dbcancel(DBPROCESS *dbproc);
extern RETCODE dbcanquery(DBPROCESS *dbproc);
extern char *dbchange(DBPROCESS *dbprocess);
extern DBBOOL dbcharsetconv(DBPROCESS *dbprocess);
extern void dbclose(DBPROCESS *dbproc);
extern void dbclrbuf(DBPROCESS *dbproc, DBINT n);
extern RETCODE dbclropt(DBPROCESS *dbproc, int option, char *param);
extern RETCODE dbcmd(DBPROCESS *dbproc, char *cmdstring);
extern RETCODE DBCMDROW(DBPROCESS *dbproc);
extern DBBOOL dbcolbrowse(DBPROCESS *dbprocess, int colnum);
extern DBINT dbcollen(DBPROCESS *dbproc, int column);
extern char *dbcolname(DBPROCESS *dbproc, int column);
extern char *dbcolsource(DBPROCESS *dbproc, int colnum);
extern int dbcoltype(DBPROCESS *dbproc, int column);
extern DBTYPEINFO *dbcoltypeinfo(DBPROCESS *dbproc, int column);
extern DBINT dbcolutype(DBPROCESS *dbprocess, int column);
extern DBINT dbconvert(DBPROCESS *dbproc, int srctype, BYTE *src, DBINT srclen, int desttype, BYTE *dest, DBINT destlen);
extern DBINT dbconvert_ps(DBPROCESS *dbprocess, int srctype, BYTE *src, DBINT srclen, int desttype, BYTE *dest, DBINT destlen, DBTYPEINFO *typeinfo);
extern DBINT DBCOUNT(DBPROCESS *dbproc);
extern int DBCURCMD(DBPROCESS *dbproc);
extern DBINT DBCURROW(DBPROCESS *dbproc);
extern RETCODE dbcursor(DBCURSOR *hc, DBINT optype, DBINT bufno, BYTE *table, BYTE *values);
extern RETCODE dbcursorbind(DBCURSOR *hc, int col, int vartype, DBINT varlen, DBINT *poutlen, BYTE *pvaraddr, DBTYPEINFO *typeinfo);
extern void dbcursorclose(DBCURSOR *hc);
extern RETCODE dbcursorcolinfo(DBCURSOR *hc, DBINT column, DBCHAR *colname, DBINT *coltype, DBINT *collen, DBINT *usertype);
extern RETCODE dbcursorfetch(DBCURSOR *hc, DBINT fetchtype, DBINT rownum);
extern RETCODE dbcursorinfo(DBCURSOR *hc, DBINT *ncols, DBINT *nrows);
extern DBCURSOR *dbcursoropen(DBPROCESS *dbprocess, BYTE *stmt, SHORT scollopt, SHORT concuropt, USHORT nrows, DBINT *pstatus);
extern BYTE *dbdata(DBPROCESS *dbproc, int column);
extern int dbdate4cmp(DBPROCESS *dbprocess, DBDATETIME4 *d1, DBDATETIME4 *d2);
extern RETCODE dbdate4zero(DBPROCESS *dbprocess, DBDATETIME4 *d1);
extern RETCODE dbdatechar(DBPROCESS *dbprocess, char *buf, int datepart, int value);
extern RETCODE dbdatecmp(DBPROCESS *dbproc, DBDATETIME *d1, DBDATETIME *d2);
extern RETCODE dbdatecrack(DBPROCESS *dbproc, DBDATEREC *di, DBDATETIME *dt);
extern int dbdatename(DBPROCESS *dbprocess, char *buf, int date, DBDATETIME *datetime);
extern char *dateorder(DBPROCESS *dbprocess, char *language);
extern DBINT dbdatepart(DBPROCESS *dbprocess, int datepart, DBDATETIME *datetime);
extern RETCODE dbdatezero(DBPROCESS *dbprocess, DBDATETIME *d1);
extern DBINT dbdatlen(DBPROCESS *dbproc, int column);
extern char *dbdayname(DBPROCESS *dbprocess, char *language, int daynum);
extern DBBOOL DBDEAD(DBPROCESS *dbproc);
extern EHANDLEFUNC dberrhandle(EHANDLEFUNC handler);
extern void dbexit(void);
extern RETCODE dbfcmd(DBPROCESS *dbproc, char *fmt, ...);
extern DBINT DBFIRSTROW(DBPROCESS *dbproc);
extern RETCODE dbfree_xlate(DBPROCESS *dbprocess, DBXLATE *xlt_tosrv, DBXLATE *clt_todisp);
extern void dbfreebuf(DBPROCESS *dbproc);
extern void dbfreequal(char *qualptr);
extern RETCODE dbfreesort(DBPROCESS *dbprocess, DBSORTORDER *sortorder);
extern char *dbgetchar(DBPROCESS *dbprocess, int n);
extern char *dbgetcharset(DBPROCESS *dbprocess);
extern RETCODE dbgetloginfo(DBPROCESS *dbprocess, DBLOGINFO **loginfo);
extern int dbgetlusername(LOGINREC *login, BYTE *name_buffer, int buffer_len);
extern int dbgetmaxprocs(void);
extern char *dbgetnatlanf(DBPROCESS *dbprocess);
extern int dbgetoff(DBPROCESS *dbprocess, DBUSMALLINT offtype, int startfrom);
extern int dbgetpacket(DBPROCESS *dbproc);
extern RETCODE dbgetrow(DBPROCESS *dbproc, DBINT row);
extern int DBGETTIME(void);
extern BYTE *dbgetuserdata(DBPROCESS *dbproc);
extern DBBOOL dbhasretstat(DBPROCESS *dbproc);
extern RETCODE dbinit(void);
extern int DBIORDESC(DBPROCESS *dbproc);
extern int DBIOWDESC(DBPROCESS *dbproc);
extern DBBOOL DBISAVAIL(DBPROCESS *dbprocess);
extern DBBOOL dbisopt(DBPROCESS *dbproc, int option, char *param);
extern DBINT DBLASTROW(DBPROCESS *dbproc);
extern RETCODE dbload_xlate(DBPROCESS *dbprocess, char *srv_charset, char *clt_name, DBXLATE **xlt_tosrv, DBXLATE **xlt_todisp);
extern DBSORTORDER *dbloadsort(DBPROCESS *dbprocess);
extern LOGINREC *dblogin(void);
extern void dbloginfree(LOGINREC *login);
extern RETCODE dbmny4add(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *sum);
extern RETCODE dbmny4cmp(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2);
extern RETCODE dbmny4copy(DBPROCESS *dbprocess, DBMONEY4 *m1, DBMONEY4 *m2);
extern RETCODE dbmny4divide(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *quotient);
extern RETCODE dbmny4minus(DBPROCESS *dbproc, DBMONEY4 *src, DBMONEY4 *dest);
extern RETCODE dbmny4mul(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *prod);
extern RETCODE dbmny4sub(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *diff);
extern RETCODE dbmny4zero(DBPROCESS *dbproc, DBMONEY4 *dest);
extern RETCODE dbmnyadd(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *sum);
extern RETCODE dbmnycmp(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2);
extern RETCODE dbmnycopy(DBPROCESS *dbproc, DBMONEY *src, DBMONEY *dest);
extern RETCODE dbmnydec(DBPROCESS *dbproc, DBMONEY *mnyptr);
extern RETCODE dbmnydivide(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *quotient);
extern RETCODE dbmnydown(DBPROCESS *dbproc, DBMONEY *mnyptr, int divisor, int *remainder);
extern RETCODE dbmnyinc(DBPROCESS *dbproc, DBMONEY *mnyptr);
extern RETCODE dbmnyinit(DBPROCESS *dbproc, DBMONEY *mnyptr, int trim, DBBOOL *negative);
extern RETCODE dbmnymaxneg(DBPROCESS *dbproc, DBMONEY *dest);
extern RETCODE dbmnymaxpos(DBPROCESS *dbproc, DBMONEY *dest);
extern RETCODE dbmnyminus(DBPROCESS *dbproc, DBMONEY *src, DBMONEY *dest);
extern RETCODE dbmnymul(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *prod);
extern RETCODE dbmnydigit(DBPROCESS *dbprocess, DBMONEY *m1, DBCHAR *value, DBBOOL *zero);
extern RETCODE dbmnyscale(DBPROCESS *dbproc, DBMONEY *dest, int multiplier, int addend);
extern RETCODE dbmnysub(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *diff);
extern RETCODE dbmnyzero(DBPROCESS *dbproc, DBMONEY *dest);
extern char *dbmonthname(DBPROCESS *dbproc, char *language, int monthnum, DBBOOL shortform);
extern RETCODE DBMORECMDS(DBPROCESS *dbproc);
extern RETCODE dbmoretext(DBPROCESS *dbproc, DBINT size, BYTE *text);
extern MHANDLEFUNC dbmsghandle(MHANDLEFUNC handler);
extern char *dbname(DBPROCESS *dbproc);
extern RETCODE dbnextrow(DBPROCESS *dbproc);
extern RETCODE dbnpcreate(DBPROCESS *dbprocess);
extern RETCODE dbnpdefine(DBPROCESS *dbprocess, DBCHAR *procedure_name, DBSMALLINT namelen);
extern RETCODE dbnullbind(DBPROCESS *dbproc, int column, DBINT *indicator);
extern int dbnumalts(DBPROCESS *dbproc, int computeid);
extern int dbnumcols(DBPROCESS *dbproc);
extern int dbnumcompute(DBPROCESS *dbprocess);
extern int DBNUMORDERS(DBPROCESS *dbprocess);
extern int dbnumrets(DBPROCESS *dbproc);
extern DBPROCESS *tdsdbopen(LOGINREC *login, char *server);
extern DBPROCESS *tdsdbopen(LOGINREC *login, char *server);
extern int dbordercol(DBPROCESS *dbprocess, int order);
extern RETCODE dbpoll(DBPROCESS *dbproc, long milliseconds, DBPROCESS **ready_dbproc, int *return_reason);
extern void dbprhead(DBPROCESS *dbproc);
extern RETCODE dbprrow(DBPROCESS *dbproc);
extern char *dbprtype(int token);
extern char *dbqual(DBPROCESS *dbprocess, int tabnum, char *tabname);
extern DBBOOL DRBUF(DBPROCESS *dbprocess);
extern DBINT dbreadpage(DBPROCESS *dbprocess, char *dbname, DBINT pageno, BYTE *buf);
extern STATUS dbreadtext(DBPROCESS *dbproc, void *buf, DBINT bufsize);
extern void dbrecftos(char *filename);
extern RETCODE dbrecvpassthru(DBPROCESS *dbprocess, DBVOIDPTR *bufp);
extern RETCODE dbregdrop(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen);
extern RETCODE dbregexec(DBPROCESS *dbproc, DBUSMALLINT options);
extern RETCODE dbreghandle(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen, INTFUNCPTR handler);
extern RETCODE dbreginit(DBPROCESS *dbproc, DBCHAR *procedure_name, DBSMALLINT namelen);
extern RETCODE dbreglist(DBPROCESS *dbproc);
extern RETCODE dbregnowatch(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen);
extern RETCODE dbregparam(DBPROCESS *dbproc, char *param_name, int type, DBINT datalen, BYTE *data);
extern RETCODE dbregwatch(DBPROCESS *dbprocess, DBCHAR *procnm, DBSMALLINT namelen, DBUSMALLINT options);
extern RETCODE dbregwatchlist(DBPROCESS *dbprocess);
extern RETCODE dbresults(DBPROCESS *dbproc);
extern BYTE *dbretdata(DBPROCESS *dbproc, int retnum);
extern int dbretlen(DBPROCESS *dbproc, int retnum);
extern char *dbretname(DBPROCESS *dbproc, int retnum);
extern DBINT dbretstatus(DBPROCESS *dbproc);
extern int dbrettype(DBPROCESS *dbproc, int retnum);
extern RETCODE DBROWS(DBPROCESS *dbproc);
extern STATUS DBROWTYPE(DBPROCESS *dbprocess);
extern RETCODE dbrpcinit(DBPROCESS *dbproc, char *rpcname, DBSMALLINT options);
extern RETCODE dbrpcparam(DBPROCESS *dbproc, char *paramname, BYTE status, int type, DBINT maxlen, DBINT datalen, BYTE *value);
extern RETCODE dbrpcsend(DBPROCESS *dbproc);
extern void dbrpwclr(LOGINREC *login);
extern RETCODE dbrpwset(LOGINREC *login, char *srvname, char *password, int pwlen);
extern RETCODE dbsafestr(DBPROCESS *dbproc, char *src, DBINT srclen, char *dest, DBINT destlen, int quotetype);
extern RETCODE *dbsechandle(DBINT type, INTFUNCPTR (*handler)());
extern RETCODE dbsendpassthru(DBPROCESS *dbprocess, DBVOIDPTR bufp);
extern char *dbservcharset(DBPROCESS *dbprocess);
extern void dbsetavail(DBPROCESS *dbprocess);
extern void dbsetbusy(DBPROCESS *dbprocess, INTFUNCPTR (*handler)());
extern RETCODE dbsetdefcharset(char *charset);
extern RETCODE dbsetdeflang(char *language);
extern void dbsetidle(DBPROCESS *dbprocess, INTFUNCPTR (*handler)());
extern void dbsetifile(char *filename);
extern void dbsetinterrupt(DBPROCESS *dbproc, int (*ckintr)(void), int (*hndlintr)(void));
extern RETCODE DBSETLAPP(LOGINREC *login, char *application);
extern RETCODE DBSETLCHARSET(LOGINREC *login, char *charset);
extern RETCODE DBSETLENCRYPT(LOGINREC *loginrec, DBBOOL enable);
extern RETCODE DBSETLHOST(LOGINREC *login, char *hostname);
extern RETCODE DBSETLNATLANG(LOGINREC *loginrec, char *language);
extern RETCODE dbsetloginfo(LOGINREC *loginrec, DBLOGINFO *loginfo);
extern RETCODE dbsetlogintime(int seconds);
extern RETCODE DBSETLPACKET(LOGINREC *login, short packet_size);
extern RETCODE DBSETLPWD(LOGINREC *login, char *password);
extern RETCODE DBSETLUSER(LOGINREC *login, char *username);
extern RETCODE dbsetmaxprocs(int maxprocs);
extern RETCODE dbsetnull(DBPROCESS *dbprocess, int bindtype, int bindlen, BYTE *bindval);
extern RETCODE dbsetopt(DBPROCESS *dbproc, int option, char *char_param, int int_param);
extern STATUS dbsetrow(DBPROCESS *dbprocess, DBINT row);
extern RETCODE dbsettime(int seconds);
extern void dbsetuserdata(DBPROCESS *dbproc, BYTE *ptr);
extern RETCODE dbsetversion(DBINT version);
extern int dbspid(DBPROCESS *dbproc);
extern RETCODE dbspr1row(DBPROCESS *dbproc, char *buffer, DBINT buf_len);
extern DBINT dbspr1rowlen(DBPROCESS *dbproc);
extern RETCODE dbsprhead(DBPROCESS *dbproc, char *buffer, DBINT buf_len);
extern RETCODE dbsprline(DBPROCESS *dbproc, char *buffer, DBINT buf_len, DBCHAR line_char);
extern RETCODE dbsqlexec(DBPROCESS *dbproc);
extern RETCODE dbsqlok(DBPROCESS *dbproc);
extern RETCODE dbsqlsend(DBPROCESS *dbproc);
extern int dbstrbuild(DBPROCESS *dbprocess, char *buf, int size, char *text, char *fmt, ...);
extern int dbstrcmp(DBPROCESS *dbprocess, char *s1, int l1, char *s2, int l2, DBSORTORDER *sort);
extern RETCODE dbstrcpy(DBPROCESS *dbproc, int start, int numbytes, char *dest);
extern int dbstrlen(DBPROCESS *dbproc);
extern int dbstrsort(DBPROCESS *dbprocess, char *s1, int l1, char *s2, int l2, DBSORTORDER *sort);
extern DBBOOL dbtabbrowse(DBPROCESS *dbprocess, int tabnum);
extern int dbtabcount(DBPROCESS *dbprocess);
extern char *dbtabname(DBPROCESS *dbprocess, int tabnum);
extern char *dbtabsoruce(DBPROCESS *dbprocess, int colnum, int *tabnum);
extern int DBTDS(DBPROCESS *dbprocess);
extern DBINT dbtextsize(DBPROCESS *dbprocess);
extern int dbtsnewlen(DBPROCESS *dbprocess);
extern DBBINARY *dbtsnewval(DBPROCESS *dbprocess);
extern RETCODE dbtsput(DBPROCESS *dbprocess, DBBINARY *newts, int newtslen, int tabnum, char *tabname);
extern DBBINARY *dbtxptr(DBPROCESS *dbproc, int column);
extern DBBINARY *dbtxtimestamp(DBPROCESS *dbproc, int column);
extern DBBINARY *dbtxtsnewval(DBPROCESS *dbprocess);
extern RETCODE dbtxtsput(DBPROCESS *dbprocess, DBBINARY newtxts, int colnum);
extern RETCODE dbuse(DBPROCESS *dbproc, char *dbname);
extern DBBOOL dbcarylen(DBPROCESS *dbprocess, int column);
extern char *dbversion(void);
extern DBBOOL dbwillconvert(int srctype, int desttype);
extern RETCODE dbwritepage(DBPROCESS *dbprocess, char *dbname, DBINT pageno, DBINT size, BYTE *buf);
extern RETCODE dbwritetext(DBPROCESS *dbproc, char *objname, DBBINARY *textptr, DBTINYINT textptrlen, DBBINARY *timestamp, DBBOOL log, DBINT size, BYTE *text);
extern int dbxlate(DBPROCESS *dbprocess, char *src, int srclen, char *dest, int destlen, DBXLATE *xlt, int *srcbytes_used, DBBOOL srcend, int status);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
