
#ifndef COMMON_h
#define COMMON_h

static char rcsid_common_h[] = "$Id: common.h,v 1.11 2006-10-26 18:27:00 jklowden Exp $";
static void *no_unused_common_h_warn[] = { rcsid_common_h, no_unused_common_h_warn };

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef DBNTWIN32
#include <windows.h>
#endif

#include <sqlfront.h>
#include <sqldb.h>

#ifdef DBNTWIN32
#define MSDBLIB 1
#define MICROSOFT_DBLIB 1
#define dbloginfree(l) dbfreelogin(l)

#define SYBESMSG    SQLESMSG
#define SYBECOFL    SQLECOFL

#define SYBAOPSUM   SQLAOPSUM
#define SYBAOPMAX   SQLAOPMAX

#define SYBINT4     SQLINT4
#define SYBDATETIME SQLDATETIME
#define SYBCHAR     SQLCHAR
#define SYBVARCHAR  SQLVARCHAR
#define SYBTEXT     SQLTEXT
#define SYBBINARY   SQLBINARY
#define SYBIMAGE    SQLIMAGE

#define dberrhandle(h) dberrhandle((DBERRHANDLE_PROC) h)
#define dbmsghandle(h) dbmsghandle((DBMSGHANDLE_PROC) h)
#endif

/* cf getopt(3) */
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];

void set_malloc_options(void);
int read_login_info(int argc, char **argv);
void check_crumbs(void);
void add_bread_crumb(void);
void free_bread_crumb(void);
int syb_msg_handler(DBPROCESS * dbproc,
		    DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
int syb_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);

#endif
