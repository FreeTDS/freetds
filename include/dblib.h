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

#ifndef _dblib_h_
#define _dblib_h_

#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_dblib_h [ ] =
         "$Id: dblib.h,v 1.5 2002-09-16 19:47:58 castellano Exp $";
static void *no_unused_dblib_h_warn[]={rcsid_dblib_h, no_unused_dblib_h_warn};


#define DBLIB_INFO_MSG_TYPE 0
#define DBLIB_ERROR_MSG_TYPE 1

/*
** internal prototypes
*/
int dblib_handle_info_message(TDSCONTEXT *ctxptr, TDSSOCKET *tdsptr, TDSMSGINFO *msgptr);
int dblib_handle_err_message(TDSCONTEXT *ctxptr, TDSSOCKET *tdsptr, TDSMSGINFO *msgptr);
DBINT _convert_char(int srctype,BYTE *src,int destype,BYTE *dest,DBINT destlen);
DBINT _convert_intn(int srctype,BYTE *src,int destype,BYTE *dest,DBINT destlen);

RETCODE _bcp_clear_storage(DBPROCESS *dbproc);
RETCODE _bcp_get_term_data(FILE *hostfile, BCP_HOSTCOLINFO *hostcol, BYTE *coldata);
RETCODE _bcp_get_term_var(BYTE *dataptr, BYTE *term, int term_len, BYTE *coldata);
RETCODE _bcp_get_prog_data(DBPROCESS *dbproc);
int _bcp_readfmt_colinfo(DBPROCESS *dbproc, char *buf, BCP_HOSTCOLINFO *ci);

#ifdef __cplusplus
}
#endif

#endif
