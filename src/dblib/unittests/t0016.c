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

static char  software_version[]   = "$Id: t0016.c,v 1.1.1.1 2001-10-12 23:29:13 brianb Exp $";
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
   char				sqlCmd[256];
   char				datestring[256];
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
   
   fprintf(stdout, "About to open, PASSWORD: %s, USER: %s, SERVER: %s\n",
   	"","",""); // PASSWORD, USER, SERVER);

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
   dbcmd(dbproc, "create table dblib0016 (f1 int not null, s1 int null, f2 real null, f3 varchar(255) not null)");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   /* BCP in */

   ret = bcp_init(dbproc, "tempdb..dblib0016", in_file, err_file, DB_IN);
    
   fprintf(stderr, "select\n");
   dbcmd(dbproc, "select * from dblib0016 where 0=1"); 
   dbsqlexec(dbproc);			 
   if (dbresults(dbproc) != FAIL) {
      num_cols = dbnumcols(dbproc);
      for (i=0;i<num_cols;i++) 
         col_type[i] = dbcoltype(dbproc,i+1); 
      while (dbnextrow(dbproc) != NO_MORE_ROWS) {}
   }

   ret = bcp_columns(dbproc, num_cols);
   for (i=0;i<num_cols;i++) {
        prefix_len = 0;
	if (col_type[i]==SYBIMAGE) {
		prefix_len=4;
	} else if (!is_fixed_type(col_type[i])) {
		prefix_len=1;
	}
        bcp_colfmt(dbproc, i+1, col_type[i], prefix_len, -1, NULL, 0, i+1);
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
      for (i=0;i<num_cols;i++) 
         col_type[i] = dbcoltype(dbproc,i+1); 
      while (dbnextrow(dbproc) != NO_MORE_ROWS) {}
   }

   ret = bcp_columns(dbproc, num_cols);
   for (i=0;i<num_cols;i++) {
        prefix_len = 0;
	if (col_type[i]==SYBIMAGE) {
		prefix_len=4;
	} else if (!is_fixed_type(col_type[i])) {
		prefix_len=1;
	}
        bcp_colfmt(dbproc, i+1, col_type[i], prefix_len, -1, NULL, 0, i);
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
