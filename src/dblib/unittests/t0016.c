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

static char  software_version[]   = "$Id: t0016.c,v 1.3 2002-06-10 02:23:26 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
int failed = 0;


int main(int argc, char *argv[])
{
   const int   rows_to_add = 3;
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char        teststr[1024];
   DBINT       testint;
   char			sqlCmd[256];
   char			datestring[256];
   DBDATEREC	dateinfo;
   RETCODE      ret;
   char         *out_file = "t0016.out";
   char         *in_file = "t0016.in";
   char         *err_file = "t0016.err";
   DBINT        rows_copied;
   int          num_cols;
   int          col_type[256];
   int          prefix_len;

#ifdef __FreeBSD__
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
   dbcmd(dbproc, "drop table dblib0016");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   fprintf(stdout, "Creating table\n");
   strcpy(sqlCmd, "create table dblib0016 (f1 int not null, s1 int null, f2 numeric(10,2) null, ");
   strcat(sqlCmd, "f3 varchar(255) not null, f4 datetime null) ");
   dbcmd(dbproc, sqlCmd);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   /* BCP in */

   ret = bcp_init(dbproc, "dblib0016", in_file, err_file, DB_IN);
    
   fprintf(stdout, "return from bcp_init = %d\n", ret);

   ret = dbcmd(dbproc, "select * from dblib0016 where 0=1"); 
   fprintf(stdout, "return from dbcmd = %d\n", ret);

   ret = dbsqlexec(dbproc);			 
   fprintf(stdout, "return from dbsqlexec = %d\n", ret);

   if (dbresults(dbproc) != FAIL) {
      num_cols = dbnumcols(dbproc);
      fprintf(stdout, "Number of columns = %d\n", num_cols);

      while (dbnextrow(dbproc) != NO_MORE_ROWS) {}
   }

   ret = bcp_columns(dbproc, num_cols);
   fprintf(stdout, "return from bcp_columns = %d\n", ret);

   for (i = 1; i < num_cols ; i++ )
   {
      if ((ret = bcp_colfmt(dbproc,i, SYBCHAR, 0, -1,(BYTE *)"\t",sizeof(char) ,i)) == FAIL)
      {
          fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
      }
   }

   if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1,(BYTE *)"\n",sizeof(char) ,num_cols)) == FAIL)
   {
      fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
   }


   ret = bcp_exec(dbproc, &rows_copied);

   fprintf(stdout, "%d rows copied in\n",rows_copied);

   /* BCP out */

   rows_copied = 0;
   ret = bcp_init(dbproc, "dblib0016", out_file, err_file, DB_OUT);

   fprintf(stderr, "select\n");
   dbcmd(dbproc, "select * from dblib0016 where 0=1"); 
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
      }
   }

   if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1,(BYTE *)"\n",sizeof(char) ,num_cols)) == FAIL)
   {
      fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
   }

   ret = bcp_exec(dbproc, &rows_copied);

   fprintf(stdout, "%d rows copied out\n",rows_copied);
   dbclose(dbproc);
   dbexit();

   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}
