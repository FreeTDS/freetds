/* 
** test for proper return code from dbsqlexec()
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#define DBNTWIN32
#include <windows.h>
#endif
#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static char  software_version[]   = "$Id: t0020.c,v 1.1 2001-10-12 23:29:13 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};



int failed = 0;


int main()
{
LOGINREC   *login;
DBPROCESS   *dbproc;
int         i;
RETCODE ret;

#ifdef __FreeBSD__
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





