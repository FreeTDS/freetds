/* 
** test for proper return code from dbsqlexec()
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"
#include "tdsutil.h"

static char  software_version[]   = "$Id: t0020.c,v 1.5 2002-10-13 23:28:12 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};



int failed = 0;


int
main(int argc, char **argv)
{
LOGINREC   *login;
DBPROCESS   *dbproc;
RETCODE ret;

#if HAVE_MALLOC_OPTIONS
   /*
    * Options for malloc   A- all warnings are fatal, J- init memory to 0xD0,
    * R- always move memory block on a realloc.
    */
   extern char *malloc_options;
   malloc_options = "AJR";
#endif

   read_login_info();

#ifndef _WIN32
   tdsdump_open(NULL);
#endif

   fprintf(stdout, "Start\n");
   add_bread_crumb();

   /* Fortify_EnterScope(); */
   dbinit();

   add_bread_crumb();
   dberrhandle( syb_err_handler );
   dbmsghandle( syb_msg_handler );

   fprintf(stdout, "About to logon\n");

   add_bread_crumb();
   login = dblogin();
   DBSETLPWD(login,PASSWORD);
   DBSETLUSER(login,USER);
   DBSETLAPP(login,"t0020");

fprintf(stdout, "About to open\n");

   add_bread_crumb();
   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) dbuse(dbproc,DATABASE);
   add_bread_crumb();

   dbcmd(dbproc,"select dsjfkl dsjf");
   fprintf(stderr, "The following invalid column error is normal.\n");
   ret = dbsqlexec(dbproc);
   if (ret!=FAIL) {
         failed = 1;
         fprintf(stderr, "Failed.  Expected FAIL to be returned.\n");
         exit(1);
   }

   dbcmd(dbproc,"select db_name()");
   ret = dbsqlexec(dbproc);
   if (ret!=SUCCEED) {
         failed = 1;
         fprintf(stderr, "Failed.  Expected SUCCEED to be returned.\n");
         exit(1);
   }

   while (dbresults(dbproc)!=NO_MORE_RESULTS) {
	while (dbnextrow(dbproc)!=NO_MORE_ROWS);
   }

   add_bread_crumb();
   dbexit();
   add_bread_crumb();

   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}





