/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <tds.h>

static char  software_version[]   = "$Id: t0005.c,v 1.2 2002-08-29 09:54:54 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

int run_query(TDSSOCKET *tds, char *query);
char *value_as_string(TDSSOCKET *tds, int col_idx);

int main()
{
   TDSLOGIN *login;
   TDSSOCKET *tds;
   int verbose = 0;
   int rc;
   int i;

   char *len200 = "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
   char large_sql[1000];

   fprintf(stdout, "%s: Test large (>512 bytes) replies\n", __FILE__);
   rc = try_tds_login(&login, &tds, __FILE__, verbose);
   if (rc != TDS_SUCCEED) {
      fprintf(stderr, "try_tds_login() failed\n");
      return 1;
   }

   /* do not test error, remove always table */
   rc = run_query(tds, "DROP TABLE #test_table");
   rc = run_query(tds, "CREATE TABLE #test_table (id int, name varchar(255))");
   if (rc != TDS_SUCCEED) { return 1; }

   sprintf(large_sql, "INSERT #test_table (id, name) VALUES (0, 'A%s')", len200);
   rc = run_query(tds, large_sql);
   if (rc != TDS_SUCCEED) { return 1; }
   sprintf(large_sql, "INSERT #test_table (id, name) VALUES (1, 'B%s')", len200);
   rc = run_query(tds, large_sql);
   if (rc != TDS_SUCCEED) { return 1; }
   sprintf(large_sql, "INSERT #test_table (id, name) VALUES (2, 'C%s')", len200);
   rc = run_query(tds, large_sql);
   if (rc != TDS_SUCCEED) { return 1; }

   /*
    * The heart of the test
    */
   rc = tds_submit_query(tds, "SELECT * FROM #test_table");
   while ((rc=tds_process_result_tokens(tds))==TDS_SUCCEED) {
      while ((rc=tds_process_row_tokens(tds))==TDS_SUCCEED) {
         for (i=0; i<tds->res_info->num_cols; i++) {
            if (verbose) {
               printf("col %i is %s\n", i, value_as_string(tds, i));
            }
         }
      }
      if (rc == TDS_FAIL) {
         fprintf(stderr, "tds_process_row_tokens() returned TDS_FAIL\n");
         return 1;
      }
      else if (rc != TDS_NO_MORE_ROWS) {
         fprintf(stderr, "tds_process_row_tokens() unexpected return\n");
         return 1;
      }
   }
   if (rc == TDS_FAIL) {
      fprintf(stderr, "tds_process_result_tokens() returned TDS_FAIL for SELECT\n");
      return 1;
   }
   else if (rc != TDS_NO_MORE_RESULTS) {
      fprintf(stderr, "tds_process_result_tokens() unexpected return\n");
   }

   /* do not test error, remove always table */
   rc = run_query(tds, "DROP TABLE #test_table");

   try_tds_logout(login, tds, verbose);
   return 0;
}


/* Run query for which there should be no return results */
int run_query(TDSSOCKET *tds, char *query)
{
   int rc;

   rc = tds_submit_query(tds, query);
   if (rc != TDS_SUCCEED) {
      fprintf(stderr, "tds_submit_query() failed for query '%s'\n", query);
      return TDS_FAIL;
   }

   while ((rc=tds_process_result_tokens(tds))==TDS_SUCCEED) {
      if (tds->res_info->rows_exist) {
         fprintf(stderr, "Error:  query should not return results\n");
         return TDS_FAIL;
      }
   }
   if (rc == TDS_FAIL) {
      /* probably okay - DROP TABLE might cause this */
      /* fprintf(stderr, "tds_process_result_tokens() returned TDS_FAIL for '%s'\n", query); */
   }
   else if (rc != TDS_NO_MORE_RESULTS) {
      fprintf(stderr, "tds_process_result_tokens() unexpected return\n");
      return TDS_FAIL;
   }

  return TDS_SUCCEED;
}


char *value_as_string(
   TDSSOCKET  *tds,
   int         col_idx)
{
   static char  result[256];
   const int    type    = tds->res_info->columns[col_idx]->column_type;
   const char  *row     = tds->res_info->current_row;
   const int    offset  = tds->res_info->columns[col_idx]->column_offset;
   const void  *value   = (row+offset);

   switch(type) {
      case SYBVARCHAR:
         strncpy(result, (char *)value, sizeof(result)-1);
         result[sizeof(result)-1] = '\0';
         break;
      case SYBINT4:
         sprintf(result, "%d", *(int *)value);
         break;
      default:
         sprintf(result, "Unexpected column_type %d", type);
         break;
   }
   return result;
}
