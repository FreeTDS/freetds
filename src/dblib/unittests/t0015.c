#if HAVE_CONFIG_H
#include <config.h>
#endif

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
#include "tdsutil.h"

static char  software_version[]   = "$Id: t0015.c,v 1.5 2002-10-04 14:56:54 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};



int failed = 0;


int
main(int argc, char **argv)
{
   const int   rows_to_add = 50;
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char        teststr[1024];
   DBINT       testint;

#if HAVE_MALLOC_OPTIONS
   /*
    * Options for malloc   A- all warnings are fatal, J- init memory to 0xD0,
    * R- always move memory block on a realloc.
    */
   extern char *malloc_options;
   malloc_options = "AJR";
#endif

#ifndef _WIN32
   tdsdump_open(NULL);
#endif

   read_login_info();
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
   DBSETLAPP(login,"t0015");

fprintf(stdout, "About to open\n");

   add_bread_crumb();
   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) dbuse(dbproc,DATABASE);
   add_bread_crumb();

   fprintf(stdout, "Dropping table\n");
   add_bread_crumb();
   dbcmd(dbproc, "drop table #dblib0015");
   add_bread_crumb();
   dbsqlexec(dbproc);
   add_bread_crumb();
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }
   add_bread_crumb();

   fprintf(stdout, "creating table\n");
   dbcmd(dbproc,
         "create table #dblib0015 (i int not null, s char(10) not null)");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   fprintf(stdout, "insert\n");
   for(i=0; i<rows_to_add; i++)
   {
      char   cmd[1024];

      sprintf(cmd, "insert into #dblib0015 values (%d, 'row %03d')", i, i);
      fprintf(stdout, "%s\n",cmd);
      dbcmd(dbproc, cmd);
      dbsqlexec(dbproc);
      while (dbresults(dbproc)!=NO_MORE_RESULTS)
      {
         /* nop */
      }
   }

   fprintf(stdout, "select\n");
   dbcmd(dbproc,"select * from #dblib0015 order by i");
   dbsqlexec(dbproc);
   add_bread_crumb();


   if (dbresults(dbproc)!=SUCCEED)
   {
      add_bread_crumb();
      failed = 1;
      fprintf(stdout, "Was expecting a result set.");
      exit(1);
   }
   add_bread_crumb();

   for (i=1;i<=dbnumcols(dbproc);i++)
   {
      add_bread_crumb();
      printf ("col %d is %s\n",i,dbcolname(dbproc,i));
      add_bread_crumb();
   }

   add_bread_crumb();
   if (SUCCEED != dbbind(dbproc,1,INTBIND,-1,(BYTE *) &testint))
   {
      failed = 1;
      fprintf(stderr, "Had problem with bind\n");
      abort();
   }
   add_bread_crumb();
   if (SUCCEED != dbbind(dbproc,2,STRINGBIND,-1,(BYTE *) teststr))
   {
      failed = 1;
      fprintf(stderr, "Had problem with bind\n");
      abort();
   }
   add_bread_crumb();

   add_bread_crumb();

   if (REG_ROW != dbnextrow(dbproc))
   {
      failed = 1;
      fprintf(stderr, "Failed.  Expected a row\n");
      exit(1);
   }

   dbcancel(dbproc);

   fprintf(stdout, "select 2\n");
   dbcmd(dbproc,"select * from #dblib0015 where i > 25 order by i");
   dbsqlexec(dbproc);

   if (dbresults(dbproc)!=SUCCEED)
   {
      add_bread_crumb();
      failed = 1;
      fprintf(stdout, "Was expecting a result set.");
      exit(1);
   }

   add_bread_crumb();
   if (SUCCEED != dbbind(dbproc,1,INTBIND,-1,(BYTE *) &testint))
   {
      failed = 1;
      fprintf(stderr, "Had problem with bind\n");
      abort();
   }
   add_bread_crumb();
   if (SUCCEED != dbbind(dbproc,2,STRINGBIND,-1,(BYTE *) teststr))
   {
      failed = 1;
      fprintf(stderr, "Had problem with bind\n");
      abort();
   }
   add_bread_crumb();

   for(i=26; i<rows_to_add; i++)
   {
      char   expected[1024];
      sprintf(expected, "row %03d", i);

      add_bread_crumb();
      if (REG_ROW != dbnextrow(dbproc))
      {
         failed = 1;
         fprintf(stderr, "Failed.  Expected a row\n");
         exit(1);
      }
      add_bread_crumb();
      if (testint!=i)
      {
         failed = 1;
         fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i,
                 (int)testint);
         abort();
      }
      if (0!= strncmp(teststr, expected, strlen(expected)))
      {
         failed = 1;
         fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n",
                 expected, teststr);
         abort();
      }
      printf("Read a row of data -> %d %s\n", (int)testint, teststr);
   }

   add_bread_crumb();
   if (dbnextrow(dbproc)!=NO_MORE_ROWS)
   {
      failed = 1;
      fprintf(stderr, "Was expecting no more rows\n");
      exit(1);
   }

   add_bread_crumb();
   dbexit();
   add_bread_crumb();

   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}





