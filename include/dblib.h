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
extern "C"
{
#if 0
}
#endif
#endif

static char rcsid_dblib_h[] = "$Id: dblib.h,v 1.14 2004-01-27 21:56:44 freddy77 Exp $";
static void *no_unused_dblib_h_warn[] = { rcsid_dblib_h, no_unused_dblib_h_warn };


#define DBLIB_INFO_MSG_TYPE 0
#define DBLIB_ERROR_MSG_TYPE 1

/*
** internal prototypes
*/
int dblib_handle_info_message(TDSCONTEXT * ctxptr, TDSSOCKET * tdsptr, TDSMESSAGE* msgptr);
int dblib_handle_err_message(TDSCONTEXT * ctxptr, TDSSOCKET * tdsptr, TDSMESSAGE* msgptr);
int _dblib_client_msg(DBPROCESS * dbproc, int dberr, int severity, const char *dberrstr);
void dblib_setTDS_version(TDSLOGIN * tds_login, DBINT version);

DBINT _convert_char(int srctype, BYTE * src, int destype, BYTE * dest, DBINT destlen);
DBINT _convert_intn(int srctype, BYTE * src, int destype, BYTE * dest, DBINT destlen);

RETCODE _bcp_clear_storage(DBPROCESS * dbproc);
RETCODE _bcp_get_term_var(BYTE * dataptr, BYTE * term, int term_len);
RETCODE _bcp_get_prog_data(DBPROCESS * dbproc);
int _bcp_readfmt_colinfo(DBPROCESS * dbproc, char *buf, BCP_HOSTCOLINFO * ci);
RETCODE _bcp_read_hostfile(DBPROCESS * dbproc, FILE * hostfile, FILE * errfile, int *row_error);

extern MHANDLEFUNC _dblib_msg_handler;
extern EHANDLEFUNC _dblib_err_handler;

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
