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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <tds.h>
#include "common.h"


static char  software_version[]   = "$Id: t0002.c,v 1.4 2002-10-13 23:28:13 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

char *value_as_string(TDSSOCKET  *tds, int col_idx);

char *
value_as_string(TDSSOCKET  *tds, int col_idx)
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
} /* value_as_string()  */


int
main(int argc, char **argv)
{
   TDSLOGIN *login;
   TDSSOCKET *tds;
   int verbose = 0;
   int num_cols = 2;
   int rc;
   int i;

   fprintf(stdout, "%s: Test basic submit query, results\n", __FILE__);
   rc = try_tds_login(&login, &tds, __FILE__, verbose);
   if (rc != TDS_SUCCEED) {
      fprintf(stderr, "try_tds_login() failed\n");
      return 1;
   }

   rc = tds_submit_query(tds,"select db_name() dbname, user_name() username");
   if (rc != TDS_SUCCEED) {
      fprintf(stderr, "tds_submit_query() failed\n");
      return 1;
   }

   while ((rc=tds_process_result_tokens(tds))==TDS_SUCCEED) {
      if (tds->res_info->num_cols != num_cols) {
         fprintf(stderr, "Error:  num_cols != %d in %s\n", num_cols, __FILE__);
         return 1;
      } 
      if (tds->res_info->columns[0]->column_type != SYBVARCHAR
       || tds->res_info->columns[1]->column_type != SYBVARCHAR) {
         fprintf(stderr, "Wrong column_type in %s\n", __FILE__);
         return 1;
      }
      if (strcmp(tds->res_info->columns[0]->column_name,"dbname")
       || strcmp(tds->res_info->columns[1]->column_name,"username")) {
         fprintf(stderr, "Wrong column_name in %s\n", __FILE__);
         return 1;
      }

      while ((rc=tds_process_row_tokens(tds))==TDS_SUCCEED) {
         if (verbose) {
            for (i=0; i<num_cols; i++) {
               printf("col %i is %s\n", i, value_as_string(tds, i));
            }
         }
      }
      if (rc == TDS_FAIL) {
         fprintf(stderr, "tds_process_row_tokens() returned TDS_FAIL\n");
      }
      else if (rc != TDS_NO_MORE_ROWS) {
         fprintf(stderr, "tds_process_row_tokens() unexpected return\n");
      }
   }
   if (rc == TDS_FAIL) {
      fprintf(stderr, "tds_process_result_tokens() returned TDS_FAIL\n");
   }
   else if (rc != TDS_NO_MORE_RESULTS) {
      fprintf(stderr, "tds_process_result_tokens() unexpected return\n");
   }

   try_tds_logout(login, tds, verbose);
   return 0;
}
