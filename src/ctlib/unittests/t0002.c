#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

static char  software_version[]   = "$Id: t0002.c,v 1.1 2001-10-12 23:29:07 brianb Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

/*
 * ct_send SQL |select name = @@servername|
 * ct_bind variable
 * ct_fetch and print results
 */
int main()
{
   CS_CONTEXT *ctx; 
   CS_CONNECTION *conn; 
   CS_COMMAND *cmd; 
   int verbose = 0;

   CS_RETCODE ret;
   CS_RETCODE results_ret;
   CS_DATAFMT datafmt;
   CS_INT datalength;
   CS_SMALLINT ind;
   CS_INT count, row_count = 0;
   CS_INT result_type;
   CS_CHAR name[256];
   
   fprintf(stdout, "%s: Testing bind & select\n", __FILE__);
   if (verbose)         { fprintf(stdout, "Trying login\n"); }
   ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "Login failed\n");
     return 1;
   }

   ret = ct_command(cmd, CS_LANG_CMD, "select name = @@servername",
	CS_NULLTERM, CS_UNUSED);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "ct_command() failed\n");
     return 1;
   }
   ret = ct_send(cmd);
   if (ret != CS_SUCCEED) {
     fprintf(stderr, "ct_send() failed\n");
     return 1;
   }
   while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
      switch ((int)result_type) {
         case CS_CMD_SUCCEED:
            break;
         case CS_CMD_DONE:
            break;
         case CS_CMD_FAIL:
            fprintf(stderr,"ct_results() result_type CS_CMD_FAIL.\n");
            return 1;
         case CS_ROW_RESULT:
            datafmt.datatype = CS_CHAR_TYPE;
            datafmt.format = CS_FMT_NULLTERM;
            datafmt.maxlength = 256;
            datafmt.count = 1;
            datafmt.locale = NULL;
            ret = ct_bind(cmd, 1, &datafmt, name, &datalength, &ind);
            if (ret != CS_SUCCEED) {
              fprintf(stderr, "ct_bind() failed\n");
              return 1;
            }

            while(((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED,
             &count)) == CS_SUCCEED)
             || (ret == CS_ROW_FAIL)) {
               row_count += count;
               if (ret == CS_ROW_FAIL) {
                  fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n",
                     row_count); 
                  return 1;
               }
               else if (ret == CS_SUCCEED) {
                  if (verbose) { fprintf(stdout, "server name = %s\n", name); }
               }
               else {
                  break;
               }
            }
            switch ((int)ret) {
               case CS_END_DATA:
                  break;
               case CS_FAIL:
                  fprintf(stderr, "ct_fetch() returned CS_FAIL.\n"); 
                  return 1;
               default:
                  fprintf(stderr, "ct_fetch() unexpected return.\n"); 
                  return 1;
            }
            break;
         case CS_COMPUTE_RESULT:
            fprintf(stderr,"ct_results() unexpected CS_COMPUTE_RESULT.\n");
            return 1;
         default:
            fprintf(stderr,"ct_results() unexpected result_type.\n");
            return 1;
      }
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
