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

#ifndef _ctpublic_h_
#define _ctpublic_h_

#include <cspublic.h>

#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_ctpublic_h [ ] =
         "$Id: ctpublic.h,v 1.3 2002-09-23 23:45:29 castellano Exp $";
static void *no_unused_ctpublic_h_warn[]={rcsid_ctpublic_h, no_unused_ctpublic_h_warn};

CS_RETCODE ct_init(CS_CONTEXT *ctx, CS_INT version);
CS_RETCODE ct_con_alloc(CS_CONTEXT *ctx, CS_CONNECTION **con);
CS_RETCODE ct_con_props(CS_CONNECTION *con, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len);
CS_RETCODE ct_connect(CS_CONNECTION *con, CS_CHAR *servername, CS_INT snamelen);
CS_RETCODE ct_cmd_alloc(CS_CONNECTION *con, CS_COMMAND **cmd);
CS_RETCODE ct_cancel(CS_CONNECTION *conn, CS_COMMAND *cmd, CS_INT type);
CS_RETCODE ct_cmd_drop(CS_COMMAND *cmd);
CS_RETCODE ct_close(CS_CONNECTION *con, CS_INT option);
CS_RETCODE ct_con_drop(CS_CONNECTION *con);
CS_RETCODE ct_exit(CS_CONTEXT *ctx, CS_INT unused);
CS_RETCODE ct_command(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT option);
CS_RETCODE ct_send(CS_COMMAND *cmd);
CS_RETCODE ct_results(CS_COMMAND *cmd, CS_INT *result_type);
CS_RETCODE ct_bind(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt, CS_VOID *buffer, CS_INT *copied, CS_SMALLINT *indicator);
CS_RETCODE ct_fetch(CS_COMMAND *cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT *rows_read);
CS_RETCODE ct_res_info_dyn(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len);
CS_RETCODE ct_res_info(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len);
CS_RETCODE ct_describe(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt);
CS_RETCODE ct_callback(CS_CONTEXT *ctx, CS_CONNECTION *con, CS_INT action, CS_INT type, CS_VOID *func);

#ifdef __cplusplus
}
#endif

#endif

