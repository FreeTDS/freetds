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


static char  software_version[]   = "$Id: t0022.c,v 1.2 2002-09-13 12:55:50 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};



int main()
{
   char        cmd[1024];
   RETCODE     rc;
   const int   rows_to_add = 50;
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char        teststr[1024];
   DBINT       testint;
   int         failed = 0;
   char        *retname;
   int         rettype, retlen;

#ifdef __FreeBSD__
   /*
    * Options for malloc   A- all warnings are fatal, J- init memory to 0xD0,
    * R- always move memory block on a realloc.
    */
   extern char *malloc_options;
   malloc_options = "AJR";
#endif

   read_login_info();

   fprintf(stdout, "Start\n");
   add_bread_crumb();

   dbinit();
   
   add_bread_crumb();
   dberrhandle( syb_err_handler );
   dbmsghandle( syb_msg_handler );

   fprintf(stdout, "About to logon\n");
   
   add_bread_crumb();
   login = dblogin();
   DBSETLPWD(login,PASSWORD);
   DBSETLUSER(login,USER);
   DBSETLAPP(login,"t0022");

fprintf(stdout, "About to open\n");
   
   add_bread_crumb();
   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) dbuse(dbproc,DATABASE);
   add_bread_crumb();

   add_bread_crumb();
   
   fprintf(stdout, "Dropping table\n");
   add_bread_crumb();
   dbcmd(dbproc, "drop proc #t0022");
   add_bread_crumb();
   dbsqlexec(dbproc);
   add_bread_crumb();
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }
   add_bread_crumb();
   
   fprintf(stdout, "creating proc\n");
   dbcmd(dbproc,
         "create proc #t0022 (@b int out) as\nbegin\n select @b = 42\nend\n");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }
   
   sprintf(cmd, "declare @b int\nexec #t0022 @b = @b output\n");
   fprintf(stdout, "%s\n", cmd);
   dbcmd(dbproc, cmd);
   dbsqlexec(dbproc);
   add_bread_crumb();

   
   if (dbresults(dbproc)==FAIL) 
   {
      add_bread_crumb();
      fprintf(stdout, "Was expecting a result set.");
      exit(1);
   }
   add_bread_crumb();

   for (i=1;i<=dbnumrets(dbproc);i++)
   {
      add_bread_crumb();
      retname = dbretname(dbproc,i);
      printf ("ret name %d is %s\n",i,retname);
      rettype = dbrettype(dbproc,i);
      printf ("ret type %d is %d\n",i,rettype);
      retlen = dbretlen(dbproc,i);
      printf ("ret len %d is %d\n",i,retlen);
      dbconvert(dbproc, rettype, dbretdata(dbproc,i),
			retlen, SYBVARCHAR, teststr, -1);
      printf ("ret data %d is %s\n",i,teststr);
      add_bread_crumb();
   }
   if (strcmp(retname, "@b")) {
      fprintf(stdout, "Was expecting a retname to be @b.\n");
      exit(1);
   }
   if (strcmp(teststr, "42")) {
      fprintf(stdout, "Was expecting a retdata to be 42.\n");
      exit(1);
   }
   if (rettype!=SYBINT4) {
      fprintf(stdout, "Was expecting a rettype to be SYBINT4 was %d.\n", rettype);
      exit(1);
   }
   if (retlen!=4) {
      fprintf(stdout, "Was expecting a retlen to be 4.\n");
      exit(1);
   }
   
   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}





