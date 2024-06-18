
#ifndef COMMON_h
#define COMMON_h

#undef NDEBUG

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef DBNTWIN32
#include <freetds/windows.h>
/* fix MinGW missing declare */
#ifndef _WINDOWS_
#define _WINDOWS_ 1
#endif
#endif

#include <sqlfront.h>
#include <sqldb.h>

#include <freetds/sysdep_private.h>
#include <freetds/macros.h>

#if !defined(EXIT_FAILURE)
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#endif

#ifndef FREETDS_SRCDIR
#define FREETDS_SRCDIR FREETDS_TOPDIR "/src/dblib/unittests"
#endif

#if defined(HAVE__SNPRINTF) && !defined(HAVE_SNPRINTF)
#define snprintf _snprintf
#endif

#ifdef DBNTWIN32
/*
 * Define Sybase's symbols in terms of Microsoft's. 
 * This allows these tests to be run using Microsoft's include
 * files and library (libsybdb.lib).
 */
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
#define SYBDECIMAL  SQLDECIMAL

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
int syb_msg_handler(DBPROCESS * dbproc,
		    DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
int syb_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);

RETCODE sql_cmd(DBPROCESS *dbproc);
RETCODE sql_rewind(void);
RETCODE sql_reopen(const char *fn);

#endif
