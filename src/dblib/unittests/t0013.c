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




static char  software_version[]   = "$Id: t0013.c,v 1.4 2002-08-31 06:32:44 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
#define BLOB_BLOCK_SIZE 4096

int failed = 0;

char *testargs[] = { "", "data.bin", "t0013.out" };

int main(int argc, char *argv[])
{
   const int   rows_to_add = 3;
   LOGINREC   *login;
   DBPROCESS   *dbproc;
   int         i;
   char        teststr[1024];
   DBINT       testint;
   FILE				*fp;
   long				result, isiz;
   char				*blob, *rblob;
   unsigned char *textPtr, *timeStamp;
   char				objname[256];
   char				sqlCmd[256];
   char				rbuf[BLOB_BLOCK_SIZE];
   long				numread, numwritten, numtowrite;
   BOOL				readFirstImage;
   char   cmd[1024];

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
   DBSETLAPP(login,"t0013");
   
   fprintf(stdout, "About to open, PASSWORD: %s, USER: %s, SERVER: %s\n",
   	"","",""); /* PASSWORD, USER, SERVER); */

   dbproc = dbopen(login, SERVER);
   if (strlen(DATABASE)) {
		 dbuse(dbproc,DATABASE);
	 }
   fprintf(stdout, "After logon\n");

  fprintf(stdout, "About to read binary input file\n");

  if (argc == 1) {
	  argv = testargs;
	  argc = 3;
  }  
  if (argc < 3) {
	 fprintf(stderr, "Usage: %s infile outfile\n", argv[0]);
	 return 1;
  }

  if ((fp = fopen(argv[1], "rb")) == NULL) {
	 fprintf(stderr, "Cannot open input file: %s\n", argv[1]);
	 return 2;
  }
  result = fseek( fp, 0, SEEK_END);
  isiz = ftell(fp);
  result = fseek( fp, 0, SEEK_SET);
		      
  blob = (char *)malloc(isiz);
  fread((void *)blob, isiz, 1, fp);
  fclose (fp);

  fprintf(stdout, "Dropping table\n");
  dbcmd(dbproc, "drop table #dblib0013");
  dbsqlexec(dbproc);
  while (dbresults(dbproc)!=NO_MORE_RESULTS)
  {
      /* nop */
  }

   fprintf(stdout, "creating table\n");
   dbcmd(dbproc,
         "create table #dblib0013 (i int not null, PigTure image not null)");
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }


   fprintf(stdout, "insert\n");

   sprintf(cmd, "insert into #dblib0013 values (1, '')");
   fprintf(stdout, "%s\n",cmd);
   dbcmd(dbproc, cmd);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
   {
      /* nop */
   }

   sprintf(sqlCmd, "SELECT PigTure FROM #dblib0013 WHERE i = 1");
   dbcmd(dbproc, sqlCmd); 
   dbsqlexec(dbproc);			 
   if (dbresults(dbproc) != SUCCEED) {
   	fprintf(stderr, "Error inserting blob\n");
   	return 4;
   }
   	
   while ((result = dbnextrow(dbproc)) != NO_MORE_ROWS) {
   	result = REG_ROW ;
   	result = DBTXPLEN;
   	strcpy(objname, "#dblib0013.PigTure");
   	textPtr = dbtxptr(dbproc, 1);
   	timeStamp = dbtxtimestamp(dbproc, 1);
   }
   
   /* Use #ifdef if you want to test dbmoretext mode (needed for 16-bit apps)
      Use #ifndef for big buffer version (32-bit) */
#ifndef DBWRITE_OK_FOR_OVER_4K
   if (dbwritetext(dbproc, objname, textPtr, DBTXPLEN, timeStamp, FALSE, isiz, blob) != SUCCEED)
   return 5;
#else
   if (dbwritetext(dbproc, objname, textPtr, DBTXPLEN, timeStamp, FALSE, isiz, NULL) != SUCCEED)
      return 15;
   dbsqlok(dbproc);
   dbresults(dbproc);

   numtowrite = 0;
   /* Send the update value in chunks. */
   for (numwritten = 0; numwritten < isiz; numwritten += numtowrite) {
      numtowrite = (isiz - numwritten);
      if (numtowrite > BLOB_BLOCK_SIZE)
         numtowrite = BLOB_BLOCK_SIZE;
         dbmoretext(dbproc, (DBINT)numtowrite, blob + numwritten);
      }
      dbsqlok(dbproc);
      while (dbresults(dbproc) != NO_MORE_RESULTS);
   }
#endif

   fprintf(stdout, "select\n");

   dbcmd(dbproc,"select * from #dblib0013 order by i");
   dbsqlexec(dbproc);

   if (dbresults(dbproc)!=SUCCEED)
   {
      failed = 1;
      fprintf(stdout, "Was expecting a result set.");
      exit(1);
   }

   for (i=1;i<=dbnumcols(dbproc);i++)
   {
      printf ("col %d is %s\n",i,dbcolname(dbproc,i));
   }

   if (SUCCEED != dbbind(dbproc,1,INTBIND,-1,(BYTE *) &testint))
   {
      failed = 1;
      fprintf(stderr, "Had problem with bind\n");
      abort();
   }

   if (REG_ROW != dbnextrow(dbproc))
   {
       failed = 1;
       fprintf(stderr, "Failed.  Expected a row\n");
       exit(1);
   }
   if (testint!=1)
   {
       failed = 1;
       fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i,
               (int)testint);
       abort();
   }
   dbnextrow(dbproc);

   /* get the image */
   strcpy(sqlCmd, "SET TEXTSIZE 2147483647");
   dbcmd(dbproc, sqlCmd); 
   dbsqlexec(dbproc);			 
   dbresults(dbproc); 
			
   fprintf(stdout, "select 2\n");

   sprintf(sqlCmd, "SELECT PigTure FROM #dblib0013 WHERE i = 1");
   dbcmd(dbproc, sqlCmd); 
   dbsqlexec(dbproc);			 
   if (dbresults(dbproc) != SUCCEED) {
	fprintf(stderr, "Error extracting blob\n");
	return 6;
   }

   numread = 0;
   rblob = NULL;
   readFirstImage = FALSE;
   while ((result = dbreadtext(dbproc, rbuf, BLOB_BLOCK_SIZE)) != NO_MORE_ROWS)
   {
       if (result == 0) /* this indicates end of row */
       {
          readFirstImage = TRUE;
       } else {
          rblob = realloc(rblob, result + numread);
          memcpy((void *)(rblob + numread), (void *)rbuf, result);
					numread += result;
       }
   }

   printf("Saving first blob data row to file: %s\n", argv[2]);
   if ((fp = fopen(argv[2], "wb")) == NULL) {
       fprintf(stderr, "Unable to open output file: %s\n", argv[2]);
       return 3;
   }
   result = fwrite((void *)rblob, numread, 1, fp);
   fclose (fp);

   printf("Read blob data row %d --> %s %d byte comparison\n", 
	(int)testint, (memcmp(blob, rblob, numread)) ? "failed" 
        : "PASSED", numread);
   free(rblob);

   if (dbnextrow(dbproc)!=NO_MORE_ROWS)
   {
      failed = 1;
      fprintf(stderr, "Was expecting no more rows\n");
      exit(1);
   }

   dbexit();

   fprintf(stdout, "dblib %s on %s\n", 
           (failed?"failed!":"okay"),
           __FILE__);
   return failed ? 1 : 0; 
}





