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

static char  software_version[]   = "$Id: t0001.c,v 1.9 2002-11-06 17:00:26 castellano Exp $";
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

   set_malloc_options();

   read_login_info();

   tdsdump_open(NULL);

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
   DBSETLAPP(login,"t0001");

fprintf(stdout, "About to open\n");

   add_bread_crumb();
   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) dbuse(dbproc,DATABASE);
   add_bread_crumb();

   fprintf(stdout, "Dropping table\n");
   add_bread_crumb();
   dbcmd(dbproc, "drop table #dblib0001");
   add_bread_crumb();
   dbsqlexec(dbproc);
   add_bread_crumb();
   while (dbresults(dbproc)==SUCCEED)
   {
      /* nop */
   }
   add_bread_crumb();

   fprintf(stdout, "creating table\n");
   dbcmd(dbproc,
         "create table #dblib0001 (i int not null, s char(10) not null)");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)==SUCCEED)
   {
      /* nop */
   }

   fprintf(stdout, "insert\n");
   for(i=0; i<rows_to_add; i++)
   {
      char   cmd[1024];

      sprintf(cmd, "insert into #dblib0001 values (%d, 'row %03d')", i, i);
      fprintf(stdout, "%s\n",cmd);
      dbcmd(dbproc, cmd);
      dbsqlexec(dbproc);
      while (dbresults(dbproc)==SUCCEED)
      {
         /* nop */
      }
   }

   fprintf(stdout, "select\n");
   dbcmd(dbproc,"select * from #dblib0001 order by i");
   dbsqlexec(dbproc);
   add_bread_crumb();


   if (dbresults(dbproc)!=SUCCEED)
   {
      add_bread_crumb();
      failed = 1;
      fprintf(stdout, "Was expecting a result set.\n");
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

   for(i=0; i<rows_to_add; i++)
   {
      char   expected[1024];
      sprintf(expected, "row %03d", i);

      add_bread_crumb();
      teststr[0] = 0;
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





