#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

static char  software_version[]   = "$Id: t0004.c,v 1.1 2001-10-12 23:29:07 brianb Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

/* protos */
int do_fetch(CS_COMMAND *cmd);

/* defines */
#define NUMROWS 5

/* Testing: Test order of ct_results() */
int main()
{
   CS_CONTEXT *ctx; 
   CS_CONNECTION *conn; 
   CS_COMMAND *cmd; 
   int i, verbose = 0;

   CS_RETCODE ret;
   CS_RETCODE results_ret;
   CS_INT result_type;
   CS_INT col, num_cols;

   CS_DATAFMT datafmt;
   CS_INT datalength;
   CS_SMALLINT ind;
   char query[1024];
   CS_INT results[] = {CS_ROW_RESULT, CS_CMD_DONE, CS_END_RESULTS};
   int result_num = 0;
   
   fprintf(stdout, "%s: Check ordering of returns from cs_results()\n", __FILE__);
   if (verbose)         { fprintf(stdout, "Trying login\n"); }
   ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "Login failed\n");
     return 1;
   }

   ret = run_command(cmd, "DROP TABLE t0004");
   if (ret != CS_SUCCEED) return 1;
   ret = run_command(cmd, "CREATE TABLE t0004 (id int)");
   if (ret != CS_SUCCEED) return 1;
   for (i=0;i<NUMROWS;i++) {
	sprintf(query,"INSERT t0004 (id) VALUES (%d)",i);
   	ret = run_command(cmd, query);
   	if (ret != CS_SUCCEED) return 1;
   }

   ret = ct_command(cmd, CS_LANG_CMD,
         "UPDATE t0004 SET id = id + 1", CS_NULLTERM, CS_UNUSED);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "ct_command() failed\n");
     return 1;
   }
   ret = ct_send(cmd);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "ct_send() failed\n");
     return 1;
   }

   result_num = 0;
   while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
      if (result_type != results[result_num]) {
         fprintf(stderr, "ct_results() expected %d received %d\n",
                 results[result_num], result_type);
         return 1;
      }
      switch ((int)result_type) {
         case CS_ROW_RESULT:
            if (do_fetch(cmd)) { return 1; }
            break;
         case CS_STATUS_RESULT:
            if (do_fetch(cmd)) { return 1; }
            break;
      }
   	result_num++;
   }
   switch ((int) results_ret) {
      case CS_END_RESULTS:
         break;
      case CS_FAIL:
         fprintf(stderr,"ct_results() failed.\n");
         return 1;
         break;
      default:
         fprintf(stderr,"ct_results() unexpected return.\n");
         return 1;
   }
   
   if (verbose)         { fprintf(stdout, "Trying logout\n"); }
   ret = try_ctlogout(ctx, conn, cmd, verbose);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "Logout failed\n");
     return 1;
   }

   return 0;
}

int do_fetch(CS_COMMAND *cmd)
{
CS_INT count, row_count = 0;
CS_RETCODE ret;

	while ((ret=ct_fetch(cmd,CS_UNUSED,CS_UNUSED,CS_UNUSED,&count))==CS_SUCCEED) {
		row_count += count;
	}
	if (ret == CS_ROW_FAIL) {
		fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
		return 1;
	} else if (ret == CS_END_DATA) {
		return 0;
	} else {
		fprintf(stderr, "ct_fetch() unexpected return %d on row %d.\n", ret, row_count);
		return 1;
	}
}
