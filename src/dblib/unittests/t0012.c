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




static char  software_version[]   = "$Id: t0012.c,v 1.5 2002-09-01 06:15:29 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
int failed = 0;


int main(int argc, char *argv[])
{
   const int   rows_to_add = 3;
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char        cmd[512];
   char        teststr[1024];
   DBINT       testint;
   char				sqlCmd[256];
   char				datestring[256];
   DBDATEREC	dateinfo;
   DBDATETIME	mydatetime;

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
   DBSETLPWD(login,PASSWORD);
   DBSETLUSER(login,USER);
   DBSETLAPP(login,"t0012");
   
   fprintf(stdout, "About to open, PASSWORD: %s, USER: %s, SERVER: %s\n",
   	"","",""); /* PASSWORD, USER, SERVER); */

   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) {
		 dbuse(dbproc,DATABASE);
   }
   fprintf(stdout, "After logon\n");

   fprintf(stdout, "Dropping table\n");
   add_bread_crumb();
   dbcmd(dbproc, "drop table #dblib0012");
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
         "create table #dblib0012 (dt datetime not null)");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)==SUCCEED)
   {
      /* nop */
   }

   sprintf(cmd, "insert into #dblib0012 values ('Feb 27 2001 10:24:35:056AM')");
   fprintf(stdout, "%s\n",cmd);
   dbcmd(dbproc, cmd);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)==SUCCEED)
   {
      /* nop */
   }

   sprintf(cmd, "insert into #dblib0012 values ('Dec 25 1898 07:30:00:567PM')");
   fprintf(stdout, "%s\n",cmd);
   dbcmd(dbproc, cmd);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)==SUCCEED)
   {
      /* nop */
   }
   sprintf(sqlCmd, "SELECT dt FROM #dblib0012");
   dbcmd(dbproc, sqlCmd);
   dbsqlexec(dbproc);
   dbresults(dbproc);

   while (dbnextrow(dbproc) != NO_MORE_ROWS)
   {
	/* Print the date info  */
	dbconvert(dbproc, dbcoltype(dbproc, 1), dbdata(dbproc, 1),
		dbdatlen(dbproc, 1), SYBCHAR, datestring, -1);

	printf("%s\n",  datestring);

	/* Break up the creation date into its constituent parts */
    memcpy(&mydatetime, (DBDATETIME *) (dbdata(dbproc, 1)), sizeof(DBDATETIME));
	dbdatecrack(dbproc, &dateinfo, &mydatetime);

	/* Print the parts of the creation date */
#if MSDBLIB
	printf("\tYear = %d.\n", dateinfo.year);
	printf("\tMonth = %d.\n", dateinfo.month);
	printf("\tDay of month = %d.\n", dateinfo.day);
	printf("\tDay of year = %d.\n", dateinfo.dayofyear);
	printf("\tDay of week = %d.\n", dateinfo.weekday);
	printf("\tHour = %d.\n", dateinfo.hour);
	printf("\tMinute = %d.\n", dateinfo.minute);
	printf("\tSecond = %d.\n", dateinfo.second);
	printf("\tMillisecond = %d.\n", dateinfo.millisecond);
#else
	printf("\tYear = %d.\n", dateinfo.dateyear);
	printf("\tMonth = %d.\n", dateinfo.datemonth);
	printf("\tDay of month = %d.\n", dateinfo.datedmonth);
	printf("\tDay of year = %d.\n", dateinfo.datedyear);
	printf("\tDay of week = %d.\n", dateinfo.datedweek);
	printf("\tHour = %d.\n", dateinfo.datehour);
	printf("\tMinute = %d.\n", dateinfo.dateminute);
	printf("\tSecond = %d.\n", dateinfo.datesecond);
	printf("\tMillisecond = %d.\n", dateinfo.datemsecond);
#endif
  }

  dbclose(dbproc);
  dbexit();

  fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}
