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

#ifndef _ctlib_h_
#define _ctlib_h_
/*
** Internal (not part of the exposed API) prototypes and such.
*/
#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

static char rcsid_ctlib_h[] = "$Id: ctlib.h,v 1.9 2004-01-31 16:07:14 freddy77 Exp $";
static void *no_unused_ctlib_h_warn[] = { rcsid_ctlib_h, no_unused_ctlib_h_warn };

#include <tds.h>
/*
 * internal types
 */
struct _cs_context
{
	CS_INT date_convert_fmt;
	CS_INT cs_errhandletype;
	CS_INT cs_diag_msglimit;

	/* added for storing the maximum messages limit CT_DIAG */
	/* code changes starts here - CT_DIAG - 02 */

	CS_INT cs_diag_msglimit_client;
	CS_INT cs_diag_msglimit_server;
	CS_INT cs_diag_msglimit_total;
	struct cs_diag_msg_client *clientstore;
	struct cs_diag_msg_svr *svrstore;

	/* code changes ends here - CT_DIAG - 02 */

	struct cs_diag_msg *msgstore;
	CS_CSLIBMSG_FUNC _cslibmsg_cb;
	CS_CLIENTMSG_FUNC _clientmsg_cb;
	CS_SERVERMSG_FUNC _servermsg_cb;
/* code changes start here - CS_CONFIG - 01*/
	void *userdata;
	int userdata_len;
/* code changes end here - CS_CONFIG - 01*/
	TDSCONTEXT *tds_ctx;
	CS_CONFIG config;
};

struct _cs_blkdesc
{
	int dummy;
};

/*
 * internal typedefs
 */
typedef struct ctcolinfo
{
	TDS_SMALLINT *indicator;
}
CT_COLINFO;

/*
 * internal prototypes
 */
int _ct_handle_server_message(TDSCONTEXT * ctxptr, TDSSOCKET * tdsptr, TDSMESSAGE * msgptr);
int _ct_handle_client_message(TDSCONTEXT * ctxptr, TDSSOCKET * tdsptr, TDSMESSAGE * msgptr);
int _ct_get_server_type(int datatype);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif
