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

static char  software_version[]   = "$Id: t0016.c,v 1.10 2002-11-01 09:58:07 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
int failed = 0;


int main(int argc, char *argv[])
{
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char			sqlCmd[256];
   RETCODE      ret;
   char         *out_file = "t0016.out";
   char         *in_file = FREETDS_SRCDIR "/t0016.in";
   char         *err_file = "t0016.err";
   DBINT        rows_copied;
   int          num_cols;

#if HAVE_MALLOC_OPTIONS
   /*
    * Options for malloc   A- all warnings are fatal, J- init memory to 0xD0,
    * R- always move memory block on a realloc.
    */
   extern char *malloc_options;
   malloc_options = "AJR";
#endif

   tdsdump_open(NULL);

   read_login_info();
   fprintf(stdout, "Start\n");
   dbinit();

   dberrhandle( syb_err_handler );
   dbmsghandle( syb_msg_handler );

   fprintf(stdout, "About to logon\n");

   login = dblogin();
   BCP_SETL(login,TRUE);
   DBSETLPWD(login,PASSWORD);
   DBSETLUSER(login,USER);
   DBSETLAPP(login,"t0016");
   
   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) {
		 dbuse(dbproc,DATABASE);
   }
   fprintf(stdout, "After logon\n");

   fprintf(stdout, "Dropping table\n");
   dbcmd(dbproc, "drop table #dblib0016");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   fprintf(stdout, "Creating table\n");
   strcpy(sqlCmd, "create table #dblib0016 (f1 int not null, s1 int null, f2 numeric(10,2) null, ");
   strcat(sqlCmd, "f3 varchar(255) not null, f4 datetime null) ");
   dbcmd(dbproc, sqlCmd);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   /* BCP in */

   ret = bcp_init(dbproc, "#dblib0016", in_file, err_file, DB_IN);
   if (ret != SUCCEED)
	   failed = 1;
    
   fprintf(stdout, "return from bcp_init = %d\n", ret);

   ret = dbcmd(dbproc, "select * from #dblib0016 where 0=1"); 
   fprintf(stdout, "return from dbcmd = %d\n", ret);

   ret = dbsqlexec(dbproc);			 
   fprintf(stdout, "return from dbsqlexec = %d\n", ret);

   if (dbresults(dbproc) != FAIL) {
      num_cols = dbnumcols(dbproc);
      fprintf(stdout, "Number of columns = %d\n", num_cols);

      while (dbnextrow(dbproc) != NO_MORE_ROWS) {}
   }

   ret = bcp_columns(dbproc, num_cols);
   if (ret != SUCCEED)
	   failed = 1;
   fprintf(stdout, "return from bcp_columns = %d\n", ret);

   for (i = 1; i < num_cols ; i++ )
   {
      if ((ret = bcp_colfmt(dbproc,i, SYBCHAR, 0, -1,(BYTE *)"\t",sizeof(char) ,i)) == FAIL)
      {
          fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
	  failed = 1;
      }
   }

   if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1,(BYTE *)"\n",sizeof(char) ,num_cols)) == FAIL)
   {
      fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
      failed = 1;
   }


   ret = bcp_exec(dbproc, &rows_copied);
   if (ret != SUCCEED)
	   failed = 1;

   fprintf(stdout, "%d rows copied in\n",rows_copied);

   /* BCP out */

   rows_copied = 0;
   ret = bcp_init(dbproc, "#dblib0016", out_file, err_file, DB_OUT);
   if (ret != SUCCEED)
	   failed = 1;

   fprintf(stderr, "select\n");
   dbcmd(dbproc, "select * from #dblib0016 where 0=1"); 
   dbsqlexec(dbproc);			 

   if (dbresults(dbproc) != FAIL) {
      num_cols = dbnumcols(dbproc);
      while (dbnextrow(dbproc) != NO_MORE_ROWS) {}
   }

   ret = bcp_columns(dbproc, num_cols);

   for (i = 1; i < num_cols ; i++ )
   {
      if ((ret = bcp_colfmt(dbproc,i, SYBCHAR, 0, -1,(BYTE *)"\t",sizeof(char) ,i)) == FAIL)
      {
          fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
	  failed = 1;
      }
   }

   if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1,(BYTE *)"\n",sizeof(char) ,num_cols)) == FAIL)
   {
      fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
      failed = 1;
   }

   ret = bcp_exec(dbproc, &rows_copied);
   if (ret != SUCCEED)
	   failed = 1;

   fprintf(stdout, "%d rows copied out\n",rows_copied);
   dbclose(dbproc);
   dbexit();

   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}
