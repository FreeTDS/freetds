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

#include <config.h>
#include "ctlib.h"
#include "cspublic.h"
#include "tds.h"
/* #include "fortify.h" */


static char  software_version[]   = "$Id: ctutil.c,v 1.6 2002-07-16 05:53:07 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* all this was copied directly from the dblib functions */
int ctlib_handle_info_message(void *ctxptr,void *tdsptr, void *msgptr)
{
/* CS_CONNECTION *con = (CS_CONNECTION *)bStruct;
TDSSOCKET* tds = (TDSSOCKET *tds) con->tds_socket; */
CS_CLIENTMSG errmsg; 

	return ctlib_handle_err_message(ctxptr, tdsptr, msgptr);
/*
	memset(&errmsg,'\0',sizeof(errmsg));
	errmsg.msgnumber=tds->msg_info->msg_number;
	strcpy(errmsg.msgstring,tds->msg_info->message);
	errmsg.msgstringlen=strlen(tds->msg_info->message);
	errmsg.osnumber=0;
	errmsg.osstring[0]='\0';
	errmsg.osstringlen=0;
	if (con->_clientmsg_cb)
		con->_clientmsg_cb(con->ctx,con,&errmsg);
	else if (con->ctx->_clientmsg_cb)
		con->ctx->_clientmsg_cb(con->ctx,con,&errmsg);
*/
}

int ctlib_handle_err_message(void *ctxptr, void *tdsptr, void *msgptr)
{
TDSCONTEXT *ctx_tds = (TDSCONTEXT *) ctxptr;
TDSSOCKET *tds = (TDSSOCKET *) tdsptr;
TDSMSGINFO *msg = (TDSMSGINFO *) msgptr;
CS_SERVERMSG errmsg;
CS_CONNECTION *con = NULL;

	if (tds && tds->parent) {
		con = (CS_CONNECTION *)tds->parent;
	}

	memset(&errmsg,'\0',sizeof(errmsg));
	errmsg.msgnumber=msg->msg_number;
	strcpy(errmsg.text,msg->message);
	errmsg.state=msg->msg_state;
	errmsg.severity=msg->msg_level;
	errmsg.line=msg->line_number;
	if (msg->server) {
		errmsg.svrnlen = strlen(msg->server);
		strncpy(errmsg.svrname, msg->server, CS_MAX_NAME);
	}
	if (msg->proc_name) {
		errmsg.proclen = strlen(msg->proc_name);
		strncpy(errmsg.proc, msg->proc_name, CS_MAX_NAME);
	}
	if (!con) {
		tds_reset_msg_info(msg);
		return;
	}

	if (con->_servermsg_cb)
		con->_servermsg_cb(con->ctx,con,&errmsg);
	else if (con->ctx->_servermsg_cb)
		con->ctx->_servermsg_cb(con->ctx,con,&errmsg);
	
	tds_reset_msg_info(msg);
}
