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
         "$Id: ctpublic.h,v 1.8 2003-03-05 13:14:30 freddy77 Exp $";
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
CS_RETCODE ct_command(CS_COMMAND *cmd, CS_INT type, const CS_VOID *buffer, CS_INT buflen, CS_INT option);
CS_RETCODE ct_send(CS_COMMAND *cmd);
CS_RETCODE ct_results(CS_COMMAND *cmd, CS_INT *result_type);
CS_RETCODE ct_bind(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt, CS_VOID *buffer, CS_INT *copied, CS_SMALLINT *indicator);
CS_RETCODE ct_fetch(CS_COMMAND *cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT *rows_read);
CS_RETCODE ct_res_info_dyn(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len);
CS_RETCODE ct_res_info(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len);
CS_RETCODE ct_describe(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt);
CS_RETCODE ct_callback(CS_CONTEXT *ctx, CS_CONNECTION *con, CS_INT action, CS_INT type, CS_VOID *func);
CS_RETCODE ct_send_dyn(CS_COMMAND *cmd);
CS_RETCODE ct_results_dyn(CS_COMMAND *cmd, CS_INT *result_type);
CS_RETCODE ct_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE ct_cmd_props(CS_COMMAND *cmd, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE ct_compute_info(CS_COMMAND *cmd, CS_INT type, CS_INT colnum, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE ct_get_data(CS_COMMAND *cmd, CS_INT item, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen);
CS_RETCODE ct_send_data(CS_COMMAND *cmd, CS_VOID *buffer, CS_INT buflen);
CS_RETCODE ct_data_info(CS_COMMAND *cmd, CS_INT action, CS_INT colnum, CS_IODESC *iodesc);
CS_RETCODE ct_capability(CS_CONNECTION *con, CS_INT action, CS_INT type, CS_INT capability, CS_VOID *value);
CS_RETCODE ct_dynamic(CS_COMMAND *cmd, CS_INT type, CS_CHAR *id, CS_INT idlen, CS_CHAR *buffer, CS_INT buflen);
CS_RETCODE ct_param(CS_COMMAND *cmd, CS_DATAFMT *datafmt, CS_VOID *data, CS_INT datalen, CS_SMALLINT indicator);
CS_RETCODE ct_setparam(CS_COMMAND *cmd, CS_DATAFMT *datafmt, CS_VOID *data, CS_INT *datalen, CS_SMALLINT *indicator);
CS_RETCODE ct_options(CS_CONNECTION *con, CS_INT action, CS_INT option, CS_VOID *param, CS_INT paramlen, CS_INT *outlen);
CS_RETCODE ct_poll(CS_CONTEXT *ctx, CS_CONNECTION *connection, CS_INT milliseconds, CS_CONNECTION **compconn, CS_COMMAND **compcmd, CS_INT *compid, CS_INT *compstatus);
CS_RETCODE ct_cursor(CS_COMMAND *cmd, CS_INT type, CS_CHAR *name, CS_INT namelen, CS_CHAR *text, CS_INT tlen, CS_INT option);

#ifdef __cplusplus
}
#endif

#endif

